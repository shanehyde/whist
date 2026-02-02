#include "parse_extern_decls.h"

#include "parse_func_decl.h"
#include "parser_util.h"

Node* parse_extern_decls(Parser* parser) {
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

    while (match_token(parser, TOK_FUNC)) {
        Node* funcDeclNode = parse_func_decl(parser, 0);
        if (!funcDeclNode) {
            return NULL;
        }
        funcDeclNode->as.func_decl.is_extern = 1;
        funcDeclNode->as.func_decl.is_public = 1;
        nodelist_push(&node->as.extern_module.decls, funcDeclNode);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after extern module declarations");
    return node;
}
