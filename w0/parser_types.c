#include "parser_internal.h"

Node* parse_type(Parser* parser) {
    Token token = parser->current;

    // Pointer types are no longer supported - error if we see *
    if (check_token(parser, TOK_STAR)) {
        parse_error(parser, "Pointer types (*T) are no longer supported; use struct references");
        return NULL;
    }

    // Tuple type: (T1, T2, ...)
    if (match_token(parser, TOK_LPAREN)) {
        Node* node = node_new(NODE_TUPLE_TYPE, token.line, token.column);
        nodelist_init(&node->as.tuple_type.elem_types);

        // Parse first type
        Node* first_type = parse_type(parser);
        if (!first_type) {
            node_free(node);
            return NULL;
        }
        nodelist_push(&node->as.tuple_type.elem_types, first_type);

        // Require at least one comma (i.e., at least 2 elements for a tuple)
        if (!check_token(parser, TOK_COMMA)) {
            parse_error(parser, "Tuple type requires at least 2 elements");
            node_free(node);
            return NULL;
        }

        // Parse remaining types
        while (match_token(parser, TOK_COMMA)) {
            Node* elem_type = parse_type(parser);
            if (!elem_type) {
                node_free(node);
                return NULL;
            }
            nodelist_push(&node->as.tuple_type.elem_types, elem_type);
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' after tuple type");
        return node;
    }

    // Array type [n]type
    if (match_token(parser, TOK_LBRACKET)) {
        Node* size = NULL;
        if (!check_token(parser, TOK_RBRACKET)) {
            size = parse_expression(parser);
        }
        consume_token(parser, TOK_RBRACKET, "Expected ']' in array type");
        Node* elem = parse_type(parser);

        Node* node                    = node_new(NODE_ARRAY_TYPE, token.line, token.column);
        node->as.array_type.elem_type = elem;
        node->as.array_type.size      = size;
        return node;
    }

    // Function type: func(T1, T2) -> ReturnType
    if (match_token(parser, TOK_FUNC)) {
        Node* node = node_new(NODE_FUNC_TYPE, token.line, token.column);
        nodelist_init(&node->as.func_type.param_types);
        node->as.func_type.return_type = NULL;

        consume_token(parser, TOK_LPAREN, "Expected '(' after 'func' in type");

        if (!check_token(parser, TOK_RPAREN)) {
            do {
                Node* pt = parse_type(parser);
                if (!pt) {
                    node_free(node);
                    return NULL;
                }
                nodelist_push(&node->as.func_type.param_types, pt);
            } while (match_token(parser, TOK_COMMA));
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' in function type");

        if (match_token(parser, TOK_ARROW)) {
            node->as.func_type.return_type = parse_type(parser);
            if (!node->as.func_type.return_type) {
                node_free(node);
                return NULL;
            }
        }
        return node;
    }

    // Named type (possibly generic)
    if (match_token(parser, TOK_IDENT)) {
        // Check for generic type instantiation: Name<T1, T2, ...>
        if (check_token(parser, TOK_LT)) {
            advance_token(parser); // consume '<'

            Node* node                      = node_new(NODE_GENERIC_TYPE, token.line, token.column);
            node->as.generic_type.base_name = copy_token_string(&token);
            node->as.generic_type.base_name_length = token.length;
            nodelist_init(&node->as.generic_type.type_args);

            // Parse type arguments
            do {
                Node* type_arg = parse_type(parser);
                if (!type_arg) {
                    node_free(node);
                    return NULL;
                }
                nodelist_push(&node->as.generic_type.type_args, type_arg);
            } while (match_token(parser, TOK_COMMA));

            // Handle >> ambiguity: when we expect > but see >>, consume as single >
            if (check_token(parser, TOK_GT_GT)) {
                // Consume >> as single > by manually advancing
                // The lexer gave us >>, but we only want to consume one >
                // We'll advance past >> and remember we owe a >
                // Actually, simpler: just modify the token in place
                // Change >> to > by adjusting the current position
                parser->current.type   = TOK_GT;
                parser->current.length = 1;
            } else {
                consume_token(parser, TOK_GT, "Expected '>' after generic type arguments");
            }

            return node;
        }

        // Regular named type (not generic)
        Node* node            = node_new(NODE_IDENT, token.line, token.column);
        node->as.ident.name   = copy_token_string(&token);
        node->as.ident.length = token.length;
        return node;
    }

    parse_error(parser, "Expected type");
    return NULL;
}
