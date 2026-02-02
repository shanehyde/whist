#include "parse_struct_init.h"

#include "parse_expression.h"
#include "parser_util.h"

Node* parse_struct_init(Parser* parser) {
    Token start = parser->previous;
    Node* node  = node_new(NODE_STRUCT_INIT, start.line, start.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&node->as.struct_init.fields);

    if (!check_token(parser, TOK_RBRACE)) {
        for (;;) {
            Token field_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected field name in struct initializer");

            Node* field = node_new(NODE_FIELD_INIT, field_name.line, field_name.column);
            if (!field) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            field->as.field_init.name = copy_token_string(&field_name);
            if (!field->as.field_init.name) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            field->as.field_init.name_length = field_name.length;

            consume_token(parser, TOK_COLON, "Expected ':' after field name");
            field->as.field_init.value = parse_expression(parser);

            nodelist_push(&node->as.struct_init.fields, field);

            if (match_token(parser, TOK_COMMA)) {
                if (check_token(parser, TOK_RBRACE)) {
                    break; // trailing comma
                }
                continue;
            }
            break;
        }
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after struct initializer");
    return node;
}
