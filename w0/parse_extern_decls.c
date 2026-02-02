#include "parse_extern_decls.h"

#include "parse_func_decl.h"
#include "parser_util.h"

Node* parse_extern_decls(Parser* parser, int is_public) {
    Token module_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name string after 'extern'");

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
    consume_token(parser, TOK_LBRACE, "Expected '{' after extern module name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Parse optional function-level visibility (overrides block default)
        int func_is_public = is_public;
        if (match_token(parser, TOK_PUBLIC)) {
            func_is_public = 1;
        } else if (match_token(parser, TOK_PRIVATE)) {
            func_is_public = 0;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in extern block");
            return NULL;
        }

        Node* funcDeclNode = parse_func_decl(parser, func_is_public);
        if (!funcDeclNode) {
            return NULL;
        }
        funcDeclNode->as.func_decl.is_extern = 1;
        nodelist_push(&node->as.extern_module.decls, funcDeclNode);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after extern module declarations");
    return node;
}
