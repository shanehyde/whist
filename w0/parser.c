#include "parser.h"

#include "parse_enum_decl.h"
#include "parse_func_decl.h"
#include "parse_struct_decl.h"
#include "parse_var_decl.h"
#include "parser_util.h"

// Current recursion depth for expression parsing (declared in parser_util.h)
int parse_depth = 0;

static Node* parse_extern_decls(Parser* parser) {
    Token module_name = parser->current;
    consume(parser, TOK_IDENT, "Expected module name string after 'extern'");

    Node* node = node_new(NODE_EXTERN_MODULE, module_name.line, module_name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.extern_module.module_name = copy_token_string(&module_name);
    if (!node->as.extern_module.module_name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.extern_module.module_name_length = module_name.length;
    nodelist_init(&node->as.extern_module.decls);
    consume(parser, TOK_LBRACE, "Expected '{' after extern module name");

    while (match(parser, TOK_FUNC)) {
        Node* funcDeclNode = parse_func_decl(parser, 0);
        if (!funcDeclNode) {
            return NULL;
        }
        funcDeclNode->as.func_decl.is_extern = 1;
        funcDeclNode->as.func_decl.is_public = 1;
        nodelist_push(&node->as.extern_module.decls, funcDeclNode);
    }

    consume(parser, TOK_RBRACE, "Expected '}' after extern module declarations");
    return node;
}

static Node* parse_declaration(Parser* parser) {
    int is_public = match(parser, TOK_PUBLIC);

    if (!is_public) {
        is_public = match(parser, TOK_PRIVATE) == 0;
    }
    if (match(parser, TOK_EXTERN)) {
        return parse_extern_decls(parser);
    }
    if (match(parser, TOK_FUNC)) {
        return parse_func_decl(parser, is_public);
    }
    if (match(parser, TOK_STRUCT)) {
        return parse_struct_decl(parser, is_public);
    }
    if (match(parser, TOK_ENUM)) {
        return parse_enum_decl(parser, is_public);
    }
    if (match(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0, is_public);
    }
    if (match(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1, is_public);
    }

    parse_error(parser, "Expected declaration");
    return NULL;
}

void parser_init(Parser* parser, const char* source) {
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parse_depth          = 0; // Reset recursion depth
    advance(parser);          // Prime the parser
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    if (!program) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&program->as.program.decls);

    while (!check(parser, TOK_EOF)) {
        Node* decl = parse_declaration(parser);
        if (decl) {
            nodelist_push(&program->as.program.decls, decl);
        }
        if (parser->panic_mode)
            synchronize(parser);
    }

    return program;
}
