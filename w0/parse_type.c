#include "parse_expression.h"
#include "parser_util.h"

// Type parsing
Node* parse_type(Parser* parser) {
    Token token = parser->current;

    // Pointer types are no longer supported - error if we see *
    if (check(parser, TOK_STAR)) {
        parse_error(parser, "Pointer types (*T) are no longer supported; use struct references");
        return NULL;
    }

    // Array type [n]type
    if (match(parser, TOK_LBRACKET)) {
        Node* size = NULL;
        if (!check(parser, TOK_RBRACKET)) {
            size = parse_expression(parser);
        }
        consume(parser, TOK_RBRACKET, "Expected ']' in array type");
        Node* elem = parse_type(parser);

        Node* node = node_new(NODE_INDEX, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        node->as.index.object = elem;
        node->as.index.index  = size;
        return node;
    }

    // Named type
    if (match(parser, TOK_IDENT)) {
        Node* node = node_new(NODE_IDENT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.name = copy_token_string(&token);
        if (!node->as.ident.name) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.length = token.length;
        return node;
    }

    parse_error(parser, "Expected type");
    return NULL;
}
