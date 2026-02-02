#include "parse_enum_decl.h"

#include "parser_util.h"

Node* parse_enum_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected enum name");

    Node* node = node_new(NODE_ENUM_DECL, name.line, name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.enum_decl.is_public = is_public;
    node->as.enum_decl.name      = copy_token_string(&name);
    if (!node->as.enum_decl.name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.enum_decl.name_length = name.length;
    nodelist_init(&node->as.enum_decl.values);

    consume_token(parser, TOK_LBRACE, "Expected '{' after enum name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token value_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected enum value name");

        Node* value = node_new(NODE_IDENT, value_name.line, value_name.column);
        if (!value) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        value->as.ident.name = copy_token_string(&value_name);
        if (!value->as.ident.name) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        value->as.ident.length = value_name.length;

        nodelist_push(&node->as.enum_decl.values, value);

        if (!check_token(parser, TOK_RBRACE)) {
            consume_token(parser, TOK_COMMA, "Expected ',' or '}' after enum value");
        } else {
            match_token(parser, TOK_COMMA); // Allow trailing comma
        }
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after enum values");
    return node;
}
