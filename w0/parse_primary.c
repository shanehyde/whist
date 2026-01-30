#include "parse_primary.h"

#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

#include "parse_expression.h"
#include "parse_struct_init.h"
#include "parser_util.h"

Node* parse_primary(Parser* parser) {
    Token token = parser->current;

    if (match(parser, TOK_INT)) {
        Node* node = node_new(NODE_INT_LIT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        // Parse integer (handle hex, binary, octal)
        const char* start = token.start;
        int         base  = 10;
        if (token.length > 2) {
            if (start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
                base = 16;
                start += 2;
            } else if (start[0] == '0' && (start[1] == 'b' || start[1] == 'B')) {
                base = 2;
                start += 2;
            } else if (start[0] == '0' && (start[1] == 'o' || start[1] == 'O')) {
                base = 8;
                start += 2;
            }
        }
        errno = 0;
        char* endptr;
        long  value = strtol(start, &endptr, base);
        if (errno == ERANGE) {
            error(parser, "Integer literal out of range");
        }
        node->as.int_lit.value = value;
        return node;
    }

    if (match(parser, TOK_FLOAT)) {
        Node* node = node_new(NODE_FLOAT_LIT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.float_lit.value = strtod(token.start, NULL);
        return node;
    }

    if (match(parser, TOK_STRING)) {
        Node* node = node_new(NODE_STRING_LIT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        // Allocate max possible size (actual size may be smaller due to escapes)
        size_t max_len            = token.length - 2; // Skip quotes
        node->as.string_lit.value = malloc(max_len + 1);
        if (!node->as.string_lit.value) {
            error(parser, "Out of memory");
            return NULL;
        }
        // Process escape sequences
        const char* src = token.start + 1;
        const char* end = token.start + token.length - 1;
        char*       dst = node->as.string_lit.value;
        while (src < end) {
            if (*src == '\\' && src + 1 < end) {
                src++;
                switch (*src) {
                case 'n':
                    *dst++ = '\n';
                    break;
                case 't':
                    *dst++ = '\t';
                    break;
                case 'r':
                    *dst++ = '\r';
                    break;
                case '0':
                    *dst++ = '\0';
                    break;
                case '\\':
                    *dst++ = '\\';
                    break;
                case '"':
                    *dst++ = '"';
                    break;
                case '\'':
                    *dst++ = '\'';
                    break;
                default:
                    *dst++ = *src;
                    break;
                }
                src++;
            } else {
                *dst++ = *src++;
            }
        }
        *dst                       = '\0';
        node->as.string_lit.length = dst - node->as.string_lit.value;
        return node;
    }

    if (match(parser, TOK_CHAR)) {
        Node* node = node_new(NODE_CHAR_LIT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        // Handle escape sequences
        if (token.start[1] == '\\') {
            switch (token.start[2]) {
            case 'n':
                node->as.char_lit.value = '\n';
                break;
            case 't':
                node->as.char_lit.value = '\t';
                break;
            case 'r':
                node->as.char_lit.value = '\r';
                break;
            case '0':
                node->as.char_lit.value = '\0';
                break;
            case '\\':
                node->as.char_lit.value = '\\';
                break;
            case '\'':
                node->as.char_lit.value = '\'';
                break;
            default:
                node->as.char_lit.value = token.start[2];
                break;
            }
        } else {
            node->as.char_lit.value = token.start[1];
        }
        return node;
    }

    if (match(parser, TOK_TRUE) || match(parser, TOK_FALSE)) {
        Node* node = node_new(NODE_BOOL_LIT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.bool_lit.value = (token.type == TOK_TRUE);
        return node;
    }

    if (match(parser, TOK_NULL)) {
        Node* node = node_new(NODE_NULL_LIT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        return node;
    }

    if (match(parser, TOK_IDENT)) {
        // Check for qualified enum value: EnumName::ValueName
        if (check(parser, TOK_COLON_COLON)) {
            Token enum_name = token;
            advance(parser); // consume ::
            Token value_name = parser->current;
            consume(parser, TOK_IDENT, "Expected enum value name after '::'");

            Node* node = node_new(NODE_ENUM_VALUE, enum_name.line, enum_name.column);
            if (!node) {
                error(parser, "Out of memory");
                return NULL;
            }
            node->as.enum_value.enum_name = copy_token_string(&enum_name);
            if (!node->as.enum_value.enum_name) {
                error(parser, "Out of memory");
                return NULL;
            }
            node->as.enum_value.enum_name_length = enum_name.length;
            node->as.enum_value.value_name       = copy_token_string(&value_name);
            if (!node->as.enum_value.value_name) {
                error(parser, "Out of memory");
                return NULL;
            }
            node->as.enum_value.value_name_length = value_name.length;
            return node;
        }

        Node* node = node_new(NODE_IDENT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.name = copy_token_string(&token);
        if (!node->as.ident.name) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.length = token.length;
        return node;
    }

    // Handle 'self' keyword as an identifier
    if (match(parser, TOK_SELF)) {
        Node* node = node_new(NODE_IDENT, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.name = strdup("self");
        if (!node->as.ident.name) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.length = 4;
        return node;
    }

    if (match(parser, TOK_LPAREN)) {
        Node* expr = parse_expression(parser);
        consume(parser, TOK_RPAREN, "Expected ')' after expression");
        return expr;
    }

    if (match(parser, TOK_LBRACE)) {
        return parse_struct_init(parser);
    }

    error(parser, "Expected expression");
    return NULL;
}
