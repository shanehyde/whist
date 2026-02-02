#include <stdlib.h>

#include "parse_enum_decl.h"
#include "parser_util.h"

Node* parse_type(Parser* parser);

Node* parse_struct_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected struct name");

    Node* node = node_new(NODE_STRUCT_DECL, name.line, name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.struct_decl.is_public = is_public;
    node->as.struct_decl.name      = copy_token_string(&name);
    if (!node->as.struct_decl.name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.struct_decl.name_length = name.length;
    nodelist_init(&node->as.struct_decl.fields);

    consume_token(parser, TOK_LBRACE, "Expected '{' after struct name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token field_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected field name");

        Node* field = node_new(NODE_FIELD, field_name.line, field_name.column);
        if (!field) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        field->as.field.name = copy_token_string(&field_name);
        if (!field->as.field.name) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        field->as.field.name_length = field_name.length;

        consume_token(parser, TOK_COLON, "Expected ':' after field name");
        field->as.field.type = parse_type(parser);

        if (!check_token(parser, TOK_RBRACE)) {
            consume_token(parser, TOK_COMMA, "Expected ',' or '}' after field");
        } else {
            match_token(parser, TOK_COMMA); // Allow trailing comma
        }

        nodelist_push(&node->as.struct_decl.fields, field);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after struct fields");
    return node;
}
