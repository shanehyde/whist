#include <stdlib.h>

#include "parse_enum_decl.h"
#include "parser_util.h"

Node* parse_type(Parser* parser);

Node* parse_struct_decl(Parser* parser) {
    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected struct name");

    Node* node = node_new(NODE_STRUCT_DECL, name.line, name.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.struct_decl.name = copy_token_string(&name);
    if (!node->as.struct_decl.name) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.struct_decl.name_length = name.length;
    nodelist_init(&node->as.struct_decl.fields);

    consume(parser, TOK_LBRACE, "Expected '{' after struct name");

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        Token field_name = parser->current;
        consume(parser, TOK_IDENT, "Expected field name");

        Node* field = node_new(NODE_FIELD, field_name.line, field_name.column);
        if (!field) {
            error(parser, "Out of memory");
            return NULL;
        }
        field->as.field.name = copy_token_string(&field_name);
        if (!field->as.field.name) {
            error(parser, "Out of memory");
            return NULL;
        }
        field->as.field.name_length = field_name.length;

        consume(parser, TOK_COLON, "Expected ':' after field name");
        field->as.field.type = parse_type(parser);

        if (!check(parser, TOK_RBRACE)) {
            consume(parser, TOK_COMMA, "Expected ',' or '}' after field");
        } else {
            match(parser, TOK_COMMA); // Allow trailing comma
        }

        nodelist_push(&node->as.struct_decl.fields, field);
    }

    consume(parser, TOK_RBRACE, "Expected '}' after struct fields");
    return node;
}
