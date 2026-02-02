#include "parse_var_decl.h"

#include "parse_expression.h"
#include "parse_type.h"
#include "parser_util.h"

Node* parse_var_decl(Parser* parser, int is_const, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected variable name");

    Node* node = node_new(NODE_VAR_DECL, name.line, name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    var_decl_node* vdn = &node->as.var_decl;

    vdn->name = copy_token_string(&name);
    if (!vdn->name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    vdn->is_public   = is_public;
    vdn->name_length = name.length;
    vdn->is_const    = is_const;
    vdn->type        = NULL;
    vdn->init        = NULL;

    // Optional type annotation
    if (match_token(parser, TOK_COLON)) {
        vdn->type = parse_type(parser);
    }

    // Optional initializer
    if (match_token(parser, TOK_EQ)) {
        vdn->init = parse_expression(parser);
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}
