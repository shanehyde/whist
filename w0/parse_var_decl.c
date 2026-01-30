#include "parse_var_decl.h"

#include "parse_expression.h"
#include "parse_type.h"
#include "parser_util.h"

Node* parse_var_decl(Parser* parser, int is_const) {
    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected variable name");

    Node* node = node_new(NODE_VAR_DECL, name.line, name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.var_decl.name = copy_token_string(&name);
    if (!node->as.var_decl.name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.var_decl.name_length = name.length;
    node->as.var_decl.is_const    = is_const;
    node->as.var_decl.type        = NULL;
    node->as.var_decl.init        = NULL;

    // Optional type annotation
    if (match(parser, TOK_COLON)) {
        node->as.var_decl.type = parse_type(parser);
    }

    // Optional initializer
    if (match(parser, TOK_EQ)) {
        node->as.var_decl.init = parse_expression(parser);
    }

    consume(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}
