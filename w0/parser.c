#include "parser.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "vec.h"

// ============================================================================
// Parser Utilities
// ============================================================================

// Decode a single escape character (the character after the backslash)
static char decode_escape(char c) {
    switch (c) {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case '\\':
        return '\\';
    case '"':
        return '"';
    case '\'':
        return '\'';
    default:
        return c;
    }
}

void advance_token(Parser* parser) {
    parser->previous = parser->current;

    for (;;) {
        parser->current = lexer_next(&parser->lexer);
        if (parser->current.type != TOK_ERROR)
            break;

        // Report lexer error - the start field contains the error message
        if (!parser->panic_mode) {
            parser->panic_mode = 1;
            parser->had_error  = 1;
            fprintf(stderr, "[line %d:%d] Error: %.*s\n", parser->current.line,
                    parser->current.column, (int)parser->current.length, parser->current.start);
        }
    }
}

int check_token(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

int match_token(Parser* parser, TokenType type) {
    if (!check_token(parser, type))
        return 0;
    advance_token(parser);
    return 1;
}

void parse_error_at(Parser* parser, Token* token, const char* message) {
    if (parser->panic_mode)
        return;
    parser->panic_mode = 1;
    parser->had_error  = 1;

    if (token->type == TOK_EOF) {
        snprintf(parser->error_msg, sizeof(parser->error_msg), "[line %d:%d] Error at end: %s",
                 token->line, token->column, message);
    } else if (token->type == TOK_ERROR) {
        snprintf(parser->error_msg, sizeof(parser->error_msg), "[line %d:%d] Error: %s",
                 token->line, token->column, message);
    } else {
        snprintf(parser->error_msg, sizeof(parser->error_msg), "[line %d:%d] Error at '%.*s': %s",
                 token->line, token->column, (int)token->length, token->start, message);
    }

    fprintf(stderr, "%s\n", parser->error_msg);
}

void parse_error(Parser* parser, const char* message) {
    parse_error_at(parser, &parser->current, message);
}

void consume_token(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance_token(parser);
        return;
    }
    parse_error(parser, message);
}

void synchronize(Parser* parser) {
    parser->panic_mode = 0;

    // Always advance at least once to ensure progress and prevent infinite loops
    if (parser->current.type != TOK_EOF) {
        advance_token(parser);
    }

    while (parser->current.type != TOK_EOF) {
        if (parser->previous.type == TOK_SEMICOLON)
            return;

        switch (parser->current.type) {
        case TOK_FUNC:
        case TOK_STRUCT:
        case TOK_ENUM:
        case TOK_TRAIT:
        case TOK_TYPE:
        case TOK_IMPL:
        case TOK_VAR:
        case TOK_CONST:
        case TOK_IF:
        case TOK_WHILE:
        case TOK_FOR:
        case TOK_MATCH:
        case TOK_RETURN:
            return;
        default:
            break;
        }
        advance_token(parser);
    }
}

char* copy_token_string(Token* token) {
    char* str = xmalloc(token->length + 1);
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

// ============================================================================
// Forward Declarations
// ============================================================================

static Node*            parse_expression(Parser* parser);
static Node*            parse_type(Parser* parser);
static Node*            parse_block(Parser* parser);
static Node*            parse_statement(Parser* parser);
static Node*            parse_var_decl(Parser* parser, int is_const, int is_public);
static DestructPattern* parse_destruct_pattern(Parser* parser);

// ============================================================================
// Type Parsing
// ============================================================================

static Node* parse_type(Parser* parser) {
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

// ============================================================================
// Struct Initializer Parsing
// ============================================================================

static Node* parse_struct_init(Parser* parser) {
    Token start = parser->previous;
    Node* node  = node_new(NODE_STRUCT_INIT, start.line, start.column);
    nodelist_init(&node->as.struct_init.fields);

    if (!check_token(parser, TOK_RBRACE)) {
        // Peek ahead to determine mode: if first token is NOT ident followed by ':',
        // parse as bare expression list (for Vec element initializers)
        int is_element_list = 0;
        if (parser->current.type != TOK_IDENT) {
            is_element_list = 1;
        } else {
            // Peek: if next token after ident is not ':', it's an element list
            // We need to check if the token after the identifier is ':'
            // Use a simple lookahead: save state, advance, check, restore
            Lexer saved_lexer   = parser->lexer;
            Token saved_current = parser->current;
            int   saved_error   = parser->had_error;
            advance_token(parser);
            if (!check_token(parser, TOK_COLON)) {
                is_element_list = 1;
            }
            // Restore parser state
            parser->lexer     = saved_lexer;
            parser->current   = saved_current;
            parser->had_error = saved_error;
        }

        if (is_element_list) {
            // Parse bare expressions (for Vec initializers like {1, 2, 3})
            for (;;) {
                Token elem_token = parser->current;
                Node* field      = node_new(NODE_FIELD_INIT, elem_token.line, elem_token.column);
                field->as.field_init.name        = NULL;
                field->as.field_init.name_length = 0;
                field->as.field_init.value       = parse_expression(parser);

                nodelist_push(&node->as.struct_init.fields, field);

                if (match_token(parser, TOK_COMMA)) {
                    if (check_token(parser, TOK_RBRACE)) {
                        break; // trailing comma
                    }
                    continue;
                }
                break;
            }
        } else {
            // Parse named fields (standard struct init: {name: expr, ...})
            for (;;) {
                Token field_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected field name in struct initializer");

                Node* field = node_new(NODE_FIELD_INIT, field_name.line, field_name.column);
                field->as.field_init.name        = copy_token_string(&field_name);
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
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after struct initializer");
    return node;
}

// ============================================================================
// String Interpolation Parsing
// ============================================================================

// Decode a single escape character for interpolated string text segments
static char interp_decode_escape(char c) {
    switch (c) {
    case 'n':
        return '\n';
    case 't':
        return '\t';
    case 'r':
        return '\r';
    case '0':
        return '\0';
    case '\\':
        return '\\';
    case '"':
        return '"';
    default:
        return c;
    }
}

// Flush accumulated text buffer as a NODE_STRING_LIT part
static void flush_text_part(NodeList* parts, char* buf, int* buf_len, int line, int column) {
    if (*buf_len == 0)
        return;
    Node* text_node                = node_new(NODE_STRING_LIT, line, column);
    text_node->as.string_lit.value = xmalloc(*buf_len + 1);
    memcpy(text_node->as.string_lit.value, buf, *buf_len);
    text_node->as.string_lit.value[*buf_len] = '\0';
    text_node->as.string_lit.length          = *buf_len;
    nodelist_push(parts, text_node);
    *buf_len = 0;
}

static Node* parse_interp_string(Parser* parser) {
    Token token = parser->previous; // TOK_INTERP_STRING already consumed

    // Raw source: token.start points to '$', so content is from start+2 to start+length-1
    const char* src = token.start + 2;                // skip $"
    const char* end = token.start + token.length - 1; // before closing "

    Node* node = node_new(NODE_STRING_INTERP, token.line, token.column);
    nodelist_init(&node->as.string_interp.parts);
    node->as.string_interp.part_types = NULL;
    node->as.string_interp.part_count = 0;

    // Text buffer for accumulating literal text
    int   buf_cap = (int)(end - src) + 1;
    char* buf     = xmalloc(buf_cap);
    int   buf_len = 0;

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            // Escape sequence in text
            src++;
            buf[buf_len++] = interp_decode_escape(*src);
            src++;
        } else if (*src == '{') {
            if (src + 1 < end && src[1] == '{') {
                // Escaped brace: {{ -> literal {
                buf[buf_len++] = '{';
                src += 2;
            } else {
                // Expression: {expr}
                src++; // skip '{'

                // Find matching '}' tracking brace depth
                const char* expr_start = src;
                int         depth      = 1;
                while (src < end && depth > 0) {
                    if (*src == '{') {
                        depth++;
                    } else if (*src == '}') {
                        depth--;
                        if (depth == 0)
                            break;
                    } else if (*src == '"') {
                        // Skip string literals inside expressions
                        src++;
                        while (src < end && *src != '"') {
                            if (*src == '\\' && src + 1 < end)
                                src++;
                            src++;
                        }
                        if (src < end)
                            src++; // skip closing "
                        continue;
                    }
                    src++;
                }

                int expr_len = (int)(src - expr_start);
                if (src < end)
                    src++; // skip '}'

                // Check for empty expression
                if (expr_len == 0) {
                    if (!parser->panic_mode) {
                        parser->panic_mode = 1;
                        parser->had_error  = 1;
                        fprintf(stderr,
                                "[line %d:%d] Error: Empty expression in string interpolation\n",
                                token.line, token.column);
                    }
                    free(buf);
                    node_free(node);
                    return NULL;
                }

                // Flush any accumulated text
                flush_text_part(&node->as.string_interp.parts, buf, &buf_len, token.line,
                                token.column);

                // Create a null-terminated copy of the expression source
                char* expr_source = xmalloc(expr_len + 2); // +1 for ';', +1 for '\0'
                memcpy(expr_source, expr_start, expr_len);
                expr_source[expr_len]     = ';';
                expr_source[expr_len + 1] = '\0';

                // Parse the expression with a sub-parser
                Parser sub_parser;
                parser_init(&sub_parser, expr_source);
                Node* expr_node = parse_expression(&sub_parser);

                if (sub_parser.had_error || !expr_node) {
                    char error_msg[256];
                    snprintf(error_msg, sizeof(error_msg),
                             "Invalid expression in string interpolation");
                    parse_error_at(parser, &token, error_msg);
                    free(expr_source);
                    free(buf);
                    node_free(expr_node);
                    node_free(node);
                    return NULL;
                }

                // Fix line/column info (sub-parser uses line 1, col 1)
                expr_node->line   = token.line;
                expr_node->column = token.column;

                nodelist_push(&node->as.string_interp.parts, expr_node);
                free(expr_source);
            }
        } else if (*src == '}' && src + 1 < end && src[1] == '}') {
            // Escaped brace: }} -> literal }
            buf[buf_len++] = '}';
            src += 2;
        } else {
            buf[buf_len++] = *src;
            src++;
        }
    }

    // Flush any remaining text
    flush_text_part(&node->as.string_interp.parts, buf, &buf_len, token.line, token.column);

    free(buf);
    node->as.string_interp.part_count = node->as.string_interp.parts.count;
    return node;
}

// ============================================================================
// Primary Expression Parsing
// ============================================================================

static Node* parse_primary_expression(Parser* parser) {
    Token token = parser->current;

    if (match_token(parser, TOK_INT)) {
        Node* node = node_new(NODE_INT_LIT, token.line, token.column);
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
            parse_error(parser, "Integer literal out of range");
        }
        node->as.int_lit.value = value;
        return node;
    }

    if (match_token(parser, TOK_FLOAT)) {
        Node* node               = node_new(NODE_FLOAT_LIT, token.line, token.column);
        node->as.float_lit.value = strtod(token.start, NULL);
        return node;
    }

    if (match_token(parser, TOK_INTERP_STRING)) {
        return parse_interp_string(parser);
    }

    if (match_token(parser, TOK_STRING)) {
        Node* node = node_new(NODE_STRING_LIT, token.line, token.column);
        // Allocate max possible size (actual size may be smaller due to escapes)
        size_t max_len            = token.length - 2; // Skip quotes
        node->as.string_lit.value = xmalloc(max_len + 1);
        // Process escape sequences
        const char* src = token.start + 1;
        const char* end = token.start + token.length - 1;
        char*       dst = node->as.string_lit.value;
        while (src < end) {
            if (*src == '\\' && src + 1 < end) {
                src++;
                *dst++ = decode_escape(*src++);
            } else {
                *dst++ = *src++;
            }
        }
        *dst                       = '\0';
        node->as.string_lit.length = dst - node->as.string_lit.value;
        return node;
    }

    if (match_token(parser, TOK_CHAR)) {
        Node* node = node_new(NODE_CHAR_LIT, token.line, token.column);
        // Handle escape sequences
        if (token.start[1] == '\\') {
            node->as.char_lit.value = decode_escape(token.start[2]);
        } else {
            node->as.char_lit.value = token.start[1];
        }
        return node;
    }

    if (match_token(parser, TOK_TRUE) || match_token(parser, TOK_FALSE)) {
        Node* node              = node_new(NODE_BOOL_LIT, token.line, token.column);
        node->as.bool_lit.value = (token.type == TOK_TRUE);
        return node;
    }

    if (match_token(parser, TOK_NULL)) {
        Node* node = node_new(NODE_NULL_LIT, token.line, token.column);
        return node;
    }

    if (match_token(parser, TOK_IDENT)) {
        // Check for qualified enum value: EnumName::ValueName
        if (check_token(parser, TOK_COLON_COLON)) {
            Token enum_name = token;
            advance_token(parser); // consume ::
            Token value_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected enum value name after '::'");

            Node* node = node_new(NODE_ENUM_VALUE, enum_name.line, enum_name.column);
            node->as.enum_value.enum_name         = copy_token_string(&enum_name);
            node->as.enum_value.enum_name_length  = enum_name.length;
            node->as.enum_value.value_name        = copy_token_string(&value_name);
            node->as.enum_value.value_name_length = value_name.length;
            nodelist_init(&node->as.enum_value.args);

            // Check for constructor args: EnumName::ValueName(expr, ...)
            if (match_token(parser, TOK_LPAREN)) {
                while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
                    Node* arg = parse_expression(parser);
                    if (!arg)
                        return NULL;
                    nodelist_push(&node->as.enum_value.args, arg);
                    if (!check_token(parser, TOK_RPAREN)) {
                        consume_token(parser, TOK_COMMA,
                                      "Expected ',' or ')' after enum constructor arg");
                    }
                }
                consume_token(parser, TOK_RPAREN, "Expected ')' after enum constructor args");
            }
            return node;
        }

        Node* node            = node_new(NODE_IDENT, token.line, token.column);
        node->as.ident.name   = copy_token_string(&token);
        node->as.ident.length = token.length;
        return node;
    }

    // Handle 'self' keyword as an identifier
    if (match_token(parser, TOK_SELF)) {
        Node* node            = node_new(NODE_IDENT, token.line, token.column);
        node->as.ident.name   = xstrdup("self");
        node->as.ident.length = 4;
        return node;
    }

    if (match_token(parser, TOK_LPAREN)) {
        Node* first = parse_expression(parser);
        if (!first) {
            return NULL;
        }

        // Check for comma - if present, this is a tuple literal
        if (match_token(parser, TOK_COMMA)) {
            // Tuple literal: (e1, e2, ...)
            Node* node = node_new(NODE_TUPLE_LIT, token.line, token.column);
            nodelist_init(&node->as.tuple_lit.elements);
            nodelist_push(&node->as.tuple_lit.elements, first);

            // Parse second element (required after comma)
            Node* elem = parse_expression(parser);
            if (!elem) {
                node_free(node);
                return NULL;
            }
            nodelist_push(&node->as.tuple_lit.elements, elem);

            // Parse remaining elements
            while (match_token(parser, TOK_COMMA)) {
                elem = parse_expression(parser);
                if (!elem) {
                    node_free(node);
                    return NULL;
                }
                nodelist_push(&node->as.tuple_lit.elements, elem);
            }

            consume_token(parser, TOK_RPAREN, "Expected ')' after tuple elements");
            return node;
        }

        // Regular parenthesized expression
        consume_token(parser, TOK_RPAREN, "Expected ')' after expression");
        return first;
    }

    // New expression: new Type { fields }
    if (match_token(parser, TOK_NEW)) {
        Token new_token = parser->previous;
        Node* type_node = parse_type(parser);
        if (!type_node) {
            return NULL;
        }
        consume_token(parser, TOK_LBRACE, "Expected '{' after type in new expression");
        Node* init = parse_struct_init(parser);
        if (!init) {
            node_free(type_node);
            return NULL;
        }
        Node* node                      = node_new(NODE_NEW_EXPR, new_token.line, new_token.column);
        node->as.new_expr.type_node     = type_node;
        node->as.new_expr.init          = init;
        node->as.new_expr.resolved_type = NULL;
        return node;
    }

    // Array literal: [e1, e2, ...]
    if (match_token(parser, TOK_LBRACKET)) {
        Node* node = node_new(NODE_ARRAY_LIT, token.line, token.column);
        nodelist_init(&node->as.array_lit.elements);
        node->as.array_lit.resolved_type = NULL;

        // Check for empty array literal []
        if (!check_token(parser, TOK_RBRACKET)) {
            // Parse first element
            Node* elem = parse_expression(parser);
            if (!elem) {
                node_free(node);
                return NULL;
            }
            nodelist_push(&node->as.array_lit.elements, elem);

            // Parse remaining elements
            while (match_token(parser, TOK_COMMA)) {
                // Allow trailing comma
                if (check_token(parser, TOK_RBRACKET)) {
                    break;
                }
                elem = parse_expression(parser);
                if (!elem) {
                    node_free(node);
                    return NULL;
                }
                nodelist_push(&node->as.array_lit.elements, elem);
            }
        }

        consume_token(parser, TOK_RBRACKET, "Expected ']' after array elements");
        return node;
    }

    parse_error(parser, "Expected expression");
    return NULL;
}

// ============================================================================
// Expression Parsing
// ============================================================================

static Node* parse_postfix(Parser* parser) {
    Node* expr = parse_primary_expression(parser);
    if (!expr)
        return NULL;

    for (;;) {
        if (match_token(parser, TOK_LPAREN)) {
            // Function call
            Node* call         = node_new(NODE_CALL, expr->line, expr->column);
            call->as.call.func = expr;
            nodelist_init(&call->as.call.args);

            if (!check_token(parser, TOK_RPAREN)) {
                do {
                    Node* arg = parse_expression(parser);
                    if (arg)
                        nodelist_push(&call->as.call.args, arg);
                } while (match_token(parser, TOK_COMMA));
            }
            consume_token(parser, TOK_RPAREN, "Expected ')' after arguments");
            expr = call;
        } else if (match_token(parser, TOK_LBRACKET)) {
            // Check for slice syntax: [:], [:end], [start:], [start:end]
            // vs regular index: [expr]

            if (check_token(parser, TOK_COLON)) {
                // [:end] or [:]
                advance_token(parser); // consume ':'
                Node* end = NULL;
                if (!check_token(parser, TOK_RBRACKET)) {
                    end = parse_expression(parser);
                }
                Node* slice            = node_new(NODE_SLICE, expr->line, expr->column);
                slice->as.slice.object = expr;
                slice->as.slice.start  = NULL;
                slice->as.slice.end    = end;
                consume_token(parser, TOK_RBRACKET, "Expected ']'");
                expr = slice;
            } else {
                Node* first = parse_expression(parser);
                if (check_token(parser, TOK_COLON)) {
                    // [start:end] or [start:]
                    advance_token(parser); // consume ':'
                    Node* end = NULL;
                    if (!check_token(parser, TOK_RBRACKET)) {
                        end = parse_expression(parser);
                    }
                    Node* slice            = node_new(NODE_SLICE, expr->line, expr->column);
                    slice->as.slice.object = expr;
                    slice->as.slice.start  = first;
                    slice->as.slice.end    = end;
                    consume_token(parser, TOK_RBRACKET, "Expected ']'");
                    expr = slice;
                } else {
                    // Regular index [expr]
                    Node* index            = node_new(NODE_INDEX, expr->line, expr->column);
                    index->as.index.object = expr;
                    index->as.index.index  = first;
                    consume_token(parser, TOK_RBRACKET, "Expected ']'");
                    expr = index;
                }
            }
        } else if (match_token(parser, TOK_DOT)) {
            // Member access
            Token name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected member name after '.'");
            Node* member             = node_new(NODE_MEMBER, expr->line, expr->column);
            member->as.member.object = expr;
            member->as.member.name   = copy_token_string(&name);
            member->as.member.length = name.length;
            member->as.member.is_ref = 0; // Set by checker
            expr                     = member;
        } else {
            break;
        }
    }

    return expr;
}

static Node* parse_unary(Parser* parser) {
    if (match_token(parser, TOK_BANG) || match_token(parser, TOK_MINUS) ||
        match_token(parser, TOK_TILDE)) {
        Token op               = parser->previous;
        Node* operand          = parse_unary(parser);
        Node* node             = node_new(NODE_UNARY, op.line, op.column);
        node->as.unary.op      = op.type;
        node->as.unary.operand = operand;
        return node;
    }
    return parse_postfix(parser);
}

static Node* parse_binary(Parser* parser, Precedence min_prec);

static Precedence get_precedence(TokenType type) {
    switch (type) {
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
        return PREC_FACTOR;
    case TOK_PLUS:
    case TOK_MINUS:
        return PREC_TERM;
    case TOK_LT_LT:
    case TOK_GT_GT:
        return PREC_SHIFT;
    case TOK_LT:
    case TOK_GT:
    case TOK_LT_EQ:
    case TOK_GT_EQ:
        return PREC_COMPARISON;
    case TOK_EQ_EQ:
    case TOK_BANG_EQ:
        return PREC_EQUALITY;
    case TOK_AMP:
        return PREC_BIT_AND;
    case TOK_CARET:
        return PREC_BIT_XOR;
    case TOK_PIPE:
        return PREC_BIT_OR;
    case TOK_AMP_AMP:
        return PREC_AND;
    case TOK_PIPE_PIPE:
        return PREC_OR;
    default:
        return PREC_NONE;
    }
}

static Node* parse_binary(Parser* parser, Precedence min_prec) {
    parser->parse_depth++;
    if (parser->parse_depth > MAX_PARSE_DEPTH) {
        parse_error(parser, "Maximum expression nesting depth exceeded");
        parser->parse_depth--;
        return NULL;
    }

    Node* left = parse_unary(parser);
    if (!left) {
        parser->parse_depth--;
        return NULL;
    }

    // Cast expressions: expr as Type (binds tighter than binary operators)
    while (match_token(parser, TOK_AS)) {
        int   cast_line = parser->previous.line;
        int   cast_col  = parser->previous.column;
        Node* target    = parse_type(parser);
        if (!target) {
            parse_error(parser, "Expected type after 'as'");
            parser->parse_depth--;
            return left;
        }
        Node* cast                       = node_new(NODE_CAST, cast_line, cast_col);
        cast->as.cast_expr.expr          = left;
        cast->as.cast_expr.type_node     = target;
        cast->as.cast_expr.resolved_type = NULL;
        left                             = cast;
    }

    while (get_precedence(parser->current.type) >= min_prec &&
           get_precedence(parser->current.type) != PREC_NONE) {
        Token op = parser->current;
        advance_token(parser);
        Precedence prec  = get_precedence(op.type);
        Node*      right = parse_binary(parser, prec + 1);

        Node* binary            = node_new(NODE_BINARY, op.line, op.column);
        binary->as.binary.op    = op.type;
        binary->as.binary.left  = left;
        binary->as.binary.right = right;
        left                    = binary;
    }

    parser->parse_depth--;
    return left;
}

static int is_assign_op(TokenType type) {
    switch (type) {
    case TOK_EQ:
    case TOK_PLUS_EQ:
    case TOK_MINUS_EQ:
    case TOK_STAR_EQ:
    case TOK_SLASH_EQ:
    case TOK_PERCENT_EQ:
    case TOK_AMP_EQ:
    case TOK_PIPE_EQ:
    case TOK_CARET_EQ:
    case TOK_LT_LT_EQ:
    case TOK_GT_GT_EQ:
        return 1;
    default:
        return 0;
    }
}

static Node* parse_expression(Parser* parser) {
    Node* expr = parse_binary(parser, PREC_OR);
    if (!expr)
        return NULL;

    if (is_assign_op(parser->current.type)) {
        Token op = parser->current;
        advance_token(parser);
        Node* value = parse_expression(parser); // Right associative

        Node* assign             = node_new(NODE_ASSIGN, op.line, op.column);
        assign->as.assign.op     = op.type;
        assign->as.assign.target = expr;
        assign->as.assign.value  = value;
        return assign;
    }

    return expr;
}

// ============================================================================
// Block Parsing
// ============================================================================

static Node* parse_block(Parser* parser) {
    Node* block = node_new(NODE_BLOCK, parser->previous.line, parser->previous.column);
    nodelist_init(&block->as.block.stmts);

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Node* stmt = parse_statement(parser);
        if (stmt)
            nodelist_push(&block->as.block.stmts, stmt);
        if (parser->panic_mode)
            synchronize(parser);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after block");
    return block;
}

// ============================================================================
// Control Flow Statement Parsing
// ============================================================================

static Node* parse_if_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'if'");
    Node* cond = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after condition");

    consume_token(parser, TOK_LBRACE, "Expected '{' after if condition");
    Node* then_block = parse_block(parser);

    Node* else_block = NULL;
    if (match_token(parser, TOK_ELSE)) {
        if (match_token(parser, TOK_IF)) {
            else_block = parse_if_stmt(parser);
        } else {
            consume_token(parser, TOK_LBRACE, "Expected '{' after 'else'");
            else_block = parse_block(parser);
        }
    }

    Node* node                  = node_new(NODE_IF, token.line, token.column);
    node->as.if_stmt.cond       = cond;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}

static Node* parse_while_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'while'");
    Node* cond = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after condition");

    consume_token(parser, TOK_LBRACE, "Expected '{' after while condition");
    Node* body = parse_block(parser);

    Node* node               = node_new(NODE_WHILE, token.line, token.column);
    node->as.while_stmt.cond = cond;
    node->as.while_stmt.body = body;
    return node;
}

static Node* parse_for_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'for'");

    // Init
    Node* init = NULL;
    if (match_token(parser, TOK_VAR)) {
        init = parse_var_decl(parser, 0, 0);
    } else if (!match_token(parser, TOK_SEMICOLON)) {
        init = parse_expression(parser);
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after for initializer");
    }

    // Condition
    Node* cond = NULL;
    if (!check_token(parser, TOK_SEMICOLON)) {
        cond = parse_expression(parser);
    }
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after for condition");

    // Post
    Node* post = NULL;
    if (!check_token(parser, TOK_RPAREN)) {
        post = parse_expression(parser);
    }
    consume_token(parser, TOK_RPAREN, "Expected ')' after for clauses");

    consume_token(parser, TOK_LBRACE, "Expected '{' after for clauses");
    Node* body = parse_block(parser);

    Node* node             = node_new(NODE_FOR, token.line, token.column);
    node->as.for_stmt.init = init;
    node->as.for_stmt.cond = cond;
    node->as.for_stmt.post = post;
    node->as.for_stmt.body = body;
    return node;
}

static Node* parse_foreach_stmt(Parser* parser) {
    Token token = parser->previous;
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'foreach'");

    // Parse: const identifier (foreach variables are immutable)
    consume_token(parser, TOK_CONST, "Expected 'const' in foreach loop");
    consume_token(parser, TOK_IDENT, "Expected identifier after 'const'");

    Token var_token = parser->previous;
    char* var_name  = copy_token_string(&var_token);

    // Parse: in
    consume_token(parser, TOK_IN, "Expected 'in' after foreach variable");

    // Parse: first expression (either range start or collection)
    Node* expr = parse_expression(parser);

    Node* start      = NULL;
    Node* end        = NULL;
    Node* step       = NULL;
    Node* collection = NULL;

    if (match_token(parser, TOK_DOT_DOT)) {
        // Range foreach: foreach (const i in start..end [by step])
        start = expr;
        end   = parse_expression(parser);

        if (match_token(parser, TOK_BY)) {
            step = parse_expression(parser);
        } else {
            // Default step is 1
            step                   = node_new(NODE_INT_LIT, token.line, token.column);
            step->as.int_lit.value = 1;
        }
    } else {
        // Collection foreach: foreach (const item in vec)
        collection = expr;
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after foreach clauses");
    consume_token(parser, TOK_LBRACE, "Expected '{' after foreach clauses");
    Node* body = parse_block(parser);

    Node* node                            = node_new(NODE_FOREACH, token.line, token.column);
    node->as.foreach_stmt.var_name        = var_name;
    node->as.foreach_stmt.var_name_length = var_token.length;
    node->as.foreach_stmt.start           = start;
    node->as.foreach_stmt.end             = end;
    node->as.foreach_stmt.step            = step;
    node->as.foreach_stmt.body            = body;
    node->as.foreach_stmt.collection      = collection;
    return node;
}

static Node* parse_return_stmt(Parser* parser) {
    Token token = parser->previous;
    Node* value = NULL;

    if (!check_token(parser, TOK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after return value");

    Node* node                 = node_new(NODE_RETURN, token.line, token.column);
    node->as.return_stmt.value = value;
    return node;
}

static Node* parse_defer_stmt(Parser* parser) {
    Token token = parser->previous;

    // Parse the deferred statement (typically an expression statement like a function call)
    Node* stmt = parse_statement(parser);

    Node* node               = node_new(NODE_DEFER, token.line, token.column);
    node->as.defer_stmt.stmt = stmt;
    return node;
}

// ============================================================================
// Match Statement Parsing
// ============================================================================

static Node* parse_match_stmt(Parser* parser) {
    Token token = parser->previous; // TOK_MATCH already consumed
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'match'");
    Node* expr = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after match expression");
    consume_token(parser, TOK_LBRACE, "Expected '{' after match expression");

    Node* node                        = node_new(NODE_MATCH, token.line, token.column);
    node->as.match_stmt.expr          = expr;
    node->as.match_stmt.resolved_type = NULL;
    nodelist_init(&node->as.match_stmt.arms);

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token arm_token             = parser->current;
        Node* arm                   = node_new(NODE_MATCH_ARM, arm_token.line, arm_token.column);
        arm->as.match_arm.enum_name = NULL;
        arm->as.match_arm.enum_name_length    = 0;
        arm->as.match_arm.variant_name        = NULL;
        arm->as.match_arm.variant_name_length = 0;
        arm->as.match_arm.bindings            = NULL;
        arm->as.match_arm.binding_count       = 0;
        arm->as.match_arm.is_wildcard         = 0;
        arm->as.match_arm.body                = NULL;

        // Parse pattern
        if (parser->current.type == TOK_IDENT && parser->current.length == 1 &&
            parser->current.start[0] == '_') {
            // Wildcard: _
            advance_token(parser);
            arm->as.match_arm.is_wildcard = 1;
        } else if (check_token(parser, TOK_IDENT)) {
            Token first_ident = parser->current;
            advance_token(parser);

            if (check_token(parser, TOK_COLON_COLON)) {
                // Qualified: EnumName::VariantName or EnumName::VariantName(bindings...)
                advance_token(parser); // consume ::
                Token variant = parser->current;
                consume_token(parser, TOK_IDENT, "Expected variant name after '::'");
                arm->as.match_arm.enum_name           = copy_token_string(&first_ident);
                arm->as.match_arm.enum_name_length    = first_ident.length;
                arm->as.match_arm.variant_name        = copy_token_string(&variant);
                arm->as.match_arm.variant_name_length = variant.length;
            } else {
                // Unqualified: VariantName or VariantName(bindings...)
                arm->as.match_arm.variant_name        = copy_token_string(&first_ident);
                arm->as.match_arm.variant_name_length = first_ident.length;
            }

            // Optional bindings: (a, b, ...)
            if (match_token(parser, TOK_LPAREN)) {
                int    capacity = 4;
                char** bindings = xmalloc(capacity * sizeof(char*));
                int    count    = 0;

                while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
                    Token binding = parser->current;
                    consume_token(parser, TOK_IDENT, "Expected binding name in match pattern");
                    if (count >= capacity) {
                        capacity *= 2;
                        bindings = xrealloc(bindings, capacity * sizeof(char*));
                    }
                    bindings[count++] = copy_token_string(&binding);
                    if (!check_token(parser, TOK_RPAREN)) {
                        consume_token(parser, TOK_COMMA, "Expected ',' or ')' after binding name");
                    }
                }
                consume_token(parser, TOK_RPAREN, "Expected ')' after bindings");
                arm->as.match_arm.bindings      = bindings;
                arm->as.match_arm.binding_count = count;
            }
        } else {
            parse_error(parser, "Expected match pattern");
            node_free(arm);
            node_free(node);
            return NULL;
        }

        // Expect =>
        consume_token(parser, TOK_FAT_ARROW, "Expected '=>' after match pattern");

        // Parse arm body: block or single statement
        if (check_token(parser, TOK_LBRACE)) {
            advance_token(parser);
            arm->as.match_arm.body = parse_block(parser);
        } else {
            arm->as.match_arm.body = parse_statement(parser);
        }

        nodelist_push(&node->as.match_stmt.arms, arm);

        // Optional comma between arms
        match_token(parser, TOK_COMMA);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after match arms");
    return node;
}

// ============================================================================
// Statement Parsing
// ============================================================================

static Node* parse_statement(Parser* parser) {
    if (match_token(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0, 0);
    }
    if (match_token(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1, 0);
    }
    if (match_token(parser, TOK_IF)) {
        return parse_if_stmt(parser);
    }
    if (match_token(parser, TOK_WHILE)) {
        return parse_while_stmt(parser);
    }
    if (match_token(parser, TOK_FOR)) {
        return parse_for_stmt(parser);
    }
    if (match_token(parser, TOK_FOREACH)) {
        return parse_foreach_stmt(parser);
    }
    if (match_token(parser, TOK_RETURN)) {
        return parse_return_stmt(parser);
    }
    if (match_token(parser, TOK_DEFER)) {
        return parse_defer_stmt(parser);
    }
    if (match_token(parser, TOK_MATCH)) {
        return parse_match_stmt(parser);
    }
    if (match_token(parser, TOK_BREAK)) {
        Token token = parser->previous;
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after 'break'");
        Node* node = node_new(NODE_BREAK, token.line, token.column);
        return node;
    }
    if (match_token(parser, TOK_CONTINUE)) {
        Token token = parser->previous;
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after 'continue'");
        Node* node = node_new(NODE_CONTINUE, token.line, token.column);
        return node;
    }
    if (match_token(parser, TOK_LBRACE)) {
        return parse_block(parser);
    }

    // Expression statement
    Node* expr = parse_expression(parser);
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after expression");

    Node* node = node_new(NODE_EXPR_STMT, expr ? expr->line : 0, expr ? expr->column : 0);
    node->as.expr_stmt.expr = expr;
    return node;
}

// ============================================================================
// Declaration Parsing
// ============================================================================

// Parse a destructuring pattern element: either an identifier or a nested tuple pattern
// Called after '(' has been consumed for the outer pattern, or recursively for nested patterns
static DestructPattern* parse_destruct_pattern_element(Parser* parser) {
    if (check_token(parser, TOK_LPAREN)) {
        // Nested tuple pattern: (a, b) or (a, (b, c))
        advance_token(parser); // consume '('

        DestructPattern* pattern = pattern_new_tuple(4);

        // Parse first element
        DestructPattern* first = parse_destruct_pattern_element(parser);
        if (!first) {
            pattern_free(pattern);
            return NULL;
        }
        pattern_tuple_push(pattern, first);

        // Parse remaining elements (comma-separated)
        while (match_token(parser, TOK_COMMA)) {
            // Grow capacity if needed
            if (pattern->as.tuple.count >= 4) {
                int new_cap = pattern->as.tuple.count * 2;
                pattern->as.tuple.elements =
                    xrealloc(pattern->as.tuple.elements, new_cap * sizeof(DestructPattern*));
            }
            DestructPattern* elem = parse_destruct_pattern_element(parser);
            if (!elem) {
                pattern_free(pattern);
                return NULL;
            }
            pattern_tuple_push(pattern, elem);
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' after nested destructuring pattern");

        // Require at least 2 elements for tuple pattern
        if (pattern->as.tuple.count < 2) {
            parse_error(parser, "Destructuring pattern requires at least 2 elements");
            pattern_free(pattern);
            return NULL;
        }

        return pattern;
    } else if (check_token(parser, TOK_IDENT)) {
        // Simple identifier
        Token name_tok = parser->current;
        advance_token(parser);
        return pattern_new_ident(name_tok.start, name_tok.length);
    } else {
        parse_error(parser, "Expected variable name or nested pattern in destructuring");
        return NULL;
    }
}

// Parse the top-level destructuring pattern: var (pattern) = ...
// Called after 'var' or 'const' keyword, when '(' is the next token
static DestructPattern* parse_destruct_pattern(Parser* parser) {
    // We expect '(' to already be the current token
    if (!check_token(parser, TOK_LPAREN)) {
        parse_error(parser, "Expected '(' for destructuring pattern");
        return NULL;
    }
    return parse_destruct_pattern_element(parser);
}

static Node* parse_var_decl(Parser* parser, int is_const, int is_public) {
    Token start = parser->current;

    // Check for destructuring: var (a, b) = ... or var (a, (b, c)) = ...
    if (check_token(parser, TOK_LPAREN)) {
        DestructPattern* pattern = parse_destruct_pattern(parser);
        if (!pattern) {
            return NULL;
        }

        Node*          node = node_new(NODE_VAR_DECL, start.line, start.column);
        var_decl_node* vdn  = &node->as.var_decl;

        vdn->name             = NULL; // NULL indicates destructuring
        vdn->name_length      = 0;
        vdn->is_public        = is_public;
        vdn->is_const         = is_const;
        vdn->type             = NULL;
        vdn->init             = NULL;
        vdn->destruct_pattern = pattern;

        // Optional type annotation
        if (match_token(parser, TOK_COLON)) {
            vdn->type = parse_type(parser);
        }

        // Initializer is required for destructuring
        if (!match_token(parser, TOK_EQ)) {
            parse_error(parser, "Destructuring declaration requires an initializer");
            node_free(node);
            return NULL;
        }
        vdn->init = parse_expression(parser);

        consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return node;
    }

    // Normal variable declaration
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected variable name");

    Node*          node = node_new(NODE_VAR_DECL, name.line, name.column);
    var_decl_node* vdn  = &node->as.var_decl;

    vdn->name             = copy_token_string(&name);
    vdn->is_public        = is_public;
    vdn->name_length      = name.length;
    vdn->is_const         = is_const;
    vdn->type             = NULL;
    vdn->init             = NULL;
    vdn->destruct_pattern = NULL;

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

static Node* parse_func_decl(Parser* parser, int is_public) {
    // Check for method receiver: func (Type) or func (const Type) or func (Box<T>)
    // or func (Pair<i32, Box<T>>)
    char*    receiver_type     = NULL;
    int      receiver_type_len = 0;
    int      receiver_is_const = 0;
    NodeList receiver_type_args;
    nodelist_init(&receiver_type_args);

    if (check_token(parser, TOK_LPAREN)) {
        advance_token(parser); // consume '('

        // Check for 'const' modifier
        if (match_token(parser, TOK_CONST)) {
            receiver_is_const = 1;
        }

        // Expect struct type name
        Token recv_type = parser->current;
        consume_token(parser, TOK_IDENT, "Expected receiver type name");
        receiver_type     = copy_token_string(&recv_type);
        receiver_type_len = recv_type.length;

        // Check for generic type args: Box<T> or Pair<K, V> or Pair<i32, Box<T>>
        if (match_token(parser, TOK_LT)) {
            do {
                // Parse full type (can be identifier, generic type, etc.)
                Node* type_arg = parse_type(parser);
                if (!type_arg) {
                    free(receiver_type);
                    return NULL;
                }
                nodelist_push(&receiver_type_args, type_arg);
            } while (match_token(parser, TOK_COMMA));

            consume_token(parser, TOK_GT, "Expected '>' after type arguments");
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' after receiver type");
    }

    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected function name");

    Node*           node = node_new(NODE_FUNC_DECL, name.line, name.column);
    func_decl_node* fdn  = &node->as.func_decl;

    fdn->is_public          = is_public;
    fdn->is_extern          = 0;
    fdn->receiver_type      = receiver_type;
    fdn->receiver_type_len  = receiver_type_len;
    fdn->receiver_is_const  = receiver_is_const;
    fdn->receiver_type_args = receiver_type_args;
    fdn->name               = copy_token_string(&name);
    fdn->name_length        = name.length;
    fdn->extern_name        = NULL;
    fdn->extern_name_length = 0;
    fdn->is_varargs         = 0;
    nodelist_init(&fdn->params);

    consume_token(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check_token(parser, TOK_RPAREN)) {
        // Check for leading ... (no named params)
        if (check_token(parser, TOK_ELLIPSIS)) {
            fdn->is_varargs = 1;
            advance_token(parser);
        } else {
            do {
                // Check for ... after a comma (varargs after named params)
                if (check_token(parser, TOK_ELLIPSIS)) {
                    fdn->is_varargs = 1;
                    advance_token(parser);
                    break;
                }

                // Check for 'const' modifier
                int param_is_const = 0;
                if (match_token(parser, TOK_CONST)) {
                    param_is_const = 1;
                }

                Token param_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected parameter name");

                Node* param          = node_new(NODE_PARAM, param_name.line, param_name.column);
                param->as.param.name = copy_token_string(&param_name);
                param->as.param.name_length = param_name.length;
                param->as.param.type        = NULL;
                param->as.param.is_const    = param_is_const;

                if (match_token(parser, TOK_COLON)) {
                    param->as.param.type = parse_type(parser);
                }

                nodelist_push(&fdn->params, param);
            } while (match_token(parser, TOK_COMMA));
        }
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after parameters");

    // Return type
    fdn->return_type = NULL;
    if (match_token(parser, TOK_COLON)) {
        fdn->return_type = parse_type(parser);
    }

    // Body
    if (check_token(parser, TOK_LBRACE) == 0) {
        // Check for 'as <alias>' renaming (extern functions only)
        if (match_token(parser, TOK_AS)) {
            Token alias = parser->current;
            consume_token(parser, TOK_IDENT, "Expected identifier after 'as'");
            fdn->extern_name        = fdn->name;
            fdn->extern_name_length = fdn->name_length;
            fdn->name               = copy_token_string(&alias);
            fdn->name_length        = alias.length;
        }
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after function declaration");
        // extern function declaration
        fdn->body = NULL;
        return node;
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' before function body");
    fdn->body = parse_block(parser);

    // Copy current file's direct imports to accessible_modules
    // This determines which library modules this function can access
    fdn->accessible_modules_count = parser->direct_imports_count;
    if (parser->direct_imports_count > 0) {
        fdn->accessible_modules = xmalloc(parser->direct_imports_count * sizeof(char*));
        for (int i = 0; i < parser->direct_imports_count; i++) {
            fdn->accessible_modules[i] = xstrdup(parser->direct_imports[i]);
        }
    } else {
        fdn->accessible_modules = NULL;
    }

    return node;
}

static Node* parse_struct_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected struct name");

    Node* node                       = node_new(NODE_STRUCT_DECL, name.line, name.column);
    node->as.struct_decl.is_public   = is_public;
    node->as.struct_decl.name        = copy_token_string(&name);
    node->as.struct_decl.name_length = name.length;
    nodelist_init(&node->as.struct_decl.fields);

    // Parse type parameters if present: struct Box<T> or struct Pair<K, V>
    node->as.struct_decl.type_params       = NULL;
    node->as.struct_decl.type_param_bounds = NULL;
    node->as.struct_decl.type_param_count  = 0;

    if (match_token(parser, TOK_LT)) {
        // Parse type parameter names
        int capacity                           = 4;
        node->as.struct_decl.type_params       = xmalloc(capacity * sizeof(char*));
        node->as.struct_decl.type_param_bounds = xmalloc(capacity * sizeof(char*));

        do {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected type parameter name");

            // Grow arrays if needed
            if (node->as.struct_decl.type_param_count >= capacity) {
                capacity *= 2;
                node->as.struct_decl.type_params =
                    xrealloc(node->as.struct_decl.type_params, capacity * sizeof(char*));
                node->as.struct_decl.type_param_bounds =
                    xrealloc(node->as.struct_decl.type_param_bounds, capacity * sizeof(char*));
            }

            node->as.struct_decl.type_params[node->as.struct_decl.type_param_count] =
                copy_token_string(&param_name);

            // Check for trait bound: T: TraitName
            if (match_token(parser, TOK_COLON)) {
                Token bound_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
                node->as.struct_decl.type_param_bounds[node->as.struct_decl.type_param_count] =
                    copy_token_string(&bound_name);
            } else {
                node->as.struct_decl.type_param_bounds[node->as.struct_decl.type_param_count] =
                    NULL;
            }

            node->as.struct_decl.type_param_count++;
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type parameters");
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' after struct name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token field_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected field name");

        Node* field                 = node_new(NODE_FIELD, field_name.line, field_name.column);
        field->as.field.name        = copy_token_string(&field_name);
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

static Node* parse_trait_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected trait name");

    Node* node                      = node_new(NODE_TRAIT_DECL, name.line, name.column);
    node->as.trait_decl.is_public   = is_public;
    node->as.trait_decl.name        = copy_token_string(&name);
    node->as.trait_decl.name_length = name.length;
    nodelist_init(&node->as.trait_decl.methods);

    consume_token(parser, TOK_LBRACE, "Expected '{' after trait name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Accept 'const func' or 'func' in trait declarations
        int method_is_const = 0;
        if (check_token(parser, TOK_CONST)) {
            advance_token(parser); // consume 'const'
            method_is_const = 1;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in trait declaration");
            return NULL;
        }
        Node* method = parse_func_decl(parser, 0);
        if (method) {
            method->as.func_decl.receiver_is_const = method_is_const;
            nodelist_push(&node->as.trait_decl.methods, method);
        }
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after trait methods");
    return node;
}

static Node* parse_impl_decl(Parser* parser) {
    Token trait_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected trait name after 'impl'");

    consume_token(parser, TOK_FOR, "Expected 'for' after trait name in impl block");

    Token type_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected type name after 'for'");

    Node* node                    = node_new(NODE_IMPL_DECL, trait_name.line, trait_name.column);
    node->as.impl_decl.trait_name = copy_token_string(&trait_name);
    node->as.impl_decl.trait_name_length = trait_name.length;
    node->as.impl_decl.type_name         = copy_token_string(&type_name);
    node->as.impl_decl.type_name_length  = type_name.length;
    nodelist_init(&node->as.impl_decl.type_args);
    nodelist_init(&node->as.impl_decl.methods);

    // Parse optional type args: impl Drop for Box<T> { ... }
    if (match_token(parser, TOK_LT)) {
        do {
            Node* type_arg = parse_type(parser);
            if (!type_arg) {
                node_free(node);
                return NULL;
            }
            nodelist_push(&node->as.impl_decl.type_args, type_arg);
        } while (match_token(parser, TOK_COMMA));
        consume_token(parser, TOK_GT, "Expected '>' after type arguments");
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' after type name in impl block");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Check for 'const func' (immutable receiver) or 'func' (mutable receiver)
        int method_is_const = 0;
        if (check_token(parser, TOK_CONST) && parser->current.type == TOK_CONST) {
            // Peek ahead: if next is 'func', this is 'const func' (const receiver method)
            // If next is something else, it's an error
            advance_token(parser); // consume 'const'
            method_is_const = 1;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in impl block");
            return NULL;
        }

        // Parse the function without a receiver (no `(Type)` prefix)
        Node* method = parse_func_decl(parser, 0);
        if (method) {
            // Fill in receiver from the impl block context
            func_decl_node* fdn    = &method->as.func_decl;
            fdn->receiver_type     = xstrdup(node->as.impl_decl.type_name);
            fdn->receiver_type_len = node->as.impl_decl.type_name_length;
            fdn->receiver_is_const = method_is_const;

            // Copy type args from impl decl to receiver
            nodelist_init(&fdn->receiver_type_args);
            for (int i = 0; i < node->as.impl_decl.type_args.count; i++) {
                nodelist_push(&fdn->receiver_type_args, node->as.impl_decl.type_args.nodes[i]);
            }

            nodelist_push(&node->as.impl_decl.methods, method);
        }
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after impl methods");
    return node;
}

static Node* parse_enum_decl(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected enum name");

    Node* node                     = node_new(NODE_ENUM_DECL, name.line, name.column);
    node->as.enum_decl.is_public   = is_public;
    node->as.enum_decl.name        = copy_token_string(&name);
    node->as.enum_decl.name_length = name.length;
    nodelist_init(&node->as.enum_decl.values);

    // Parse type parameters if present: enum Option<T> or enum Result<T, E>
    node->as.enum_decl.type_params       = NULL;
    node->as.enum_decl.type_param_bounds = NULL;
    node->as.enum_decl.type_param_count  = 0;

    if (match_token(parser, TOK_LT)) {
        int capacity                         = 4;
        node->as.enum_decl.type_params       = xmalloc(capacity * sizeof(char*));
        node->as.enum_decl.type_param_bounds = xmalloc(capacity * sizeof(char*));

        do {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected type parameter name");

            if (node->as.enum_decl.type_param_count >= capacity) {
                capacity *= 2;
                node->as.enum_decl.type_params =
                    xrealloc(node->as.enum_decl.type_params, capacity * sizeof(char*));
                node->as.enum_decl.type_param_bounds =
                    xrealloc(node->as.enum_decl.type_param_bounds, capacity * sizeof(char*));
            }

            node->as.enum_decl.type_params[node->as.enum_decl.type_param_count] =
                copy_token_string(&param_name);

            if (match_token(parser, TOK_COLON)) {
                Token bound_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
                node->as.enum_decl.type_param_bounds[node->as.enum_decl.type_param_count] =
                    copy_token_string(&bound_name);
            } else {
                node->as.enum_decl.type_param_bounds[node->as.enum_decl.type_param_count] = NULL;
            }

            node->as.enum_decl.type_param_count++;
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type parameters");
    }

    consume_token(parser, TOK_LBRACE, "Expected '{' after enum name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        Token value_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected enum value name");

        Node* value = node_new(NODE_ENUM_VARIANT, value_name.line, value_name.column);
        value->as.enum_variant.name        = copy_token_string(&value_name);
        value->as.enum_variant.name_length = value_name.length;
        nodelist_init(&value->as.enum_variant.types);

        // Check for payload types: VariantName(Type1, Type2, ...)
        if (match_token(parser, TOK_LPAREN)) {
            while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
                Node* type_node = parse_type(parser);
                if (!type_node)
                    return NULL;
                nodelist_push(&value->as.enum_variant.types, type_node);
                if (!check_token(parser, TOK_RPAREN)) {
                    consume_token(parser, TOK_COMMA, "Expected ',' or ')' after variant type");
                }
            }
            consume_token(parser, TOK_RPAREN, "Expected ')' after variant types");
        }

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

static Node* parse_type_alias(Parser* parser, int is_public) {
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected type alias name");

    Node* node                      = node_new(NODE_TYPE_ALIAS, name.line, name.column);
    node->as.type_alias.is_public   = is_public;
    node->as.type_alias.name        = copy_token_string(&name);
    node->as.type_alias.name_length = name.length;

    // Parse optional type parameters: type StringMap<V> = ...
    node->as.type_alias.type_params       = NULL;
    node->as.type_alias.type_param_bounds = NULL;
    node->as.type_alias.type_param_count  = 0;

    if (match_token(parser, TOK_LT)) {
        int capacity                          = 4;
        node->as.type_alias.type_params       = xmalloc(capacity * sizeof(char*));
        node->as.type_alias.type_param_bounds = xmalloc(capacity * sizeof(char*));

        do {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected type parameter name");

            if (node->as.type_alias.type_param_count >= capacity) {
                capacity *= 2;
                node->as.type_alias.type_params =
                    xrealloc(node->as.type_alias.type_params, capacity * sizeof(char*));
                node->as.type_alias.type_param_bounds =
                    xrealloc(node->as.type_alias.type_param_bounds, capacity * sizeof(char*));
            }

            node->as.type_alias.type_params[node->as.type_alias.type_param_count] =
                copy_token_string(&param_name);

            // Check for trait bound: V: TraitName
            if (match_token(parser, TOK_COLON)) {
                Token bound_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
                node->as.type_alias.type_param_bounds[node->as.type_alias.type_param_count] =
                    copy_token_string(&bound_name);
            } else {
                node->as.type_alias.type_param_bounds[node->as.type_alias.type_param_count] = NULL;
            }

            node->as.type_alias.type_param_count++;
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type parameters");
    }

    consume_token(parser, TOK_EQ, "Expected '=' after type alias name");
    node->as.type_alias.target_type = parse_type(parser);
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after type alias");
    return node;
}

static Node* parse_extern_decls(Parser* parser, int is_public) {
    Token module_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name string after 'extern'");

    Node* node = node_new(NODE_EXTERN_MODULE, module_name.line, module_name.column);
    node->as.extern_module.module_name        = copy_token_string(&module_name);
    node->as.extern_module.module_name_length = module_name.length;
    nodelist_init(&node->as.extern_module.decls);
    consume_token(parser, TOK_LBRACE, "Expected '{' after extern module name");

    while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
        // Parse optional function-level visibility (overrides block default)
        int func_is_public = is_public;
        if (match_token(parser, TOK_PUBLIC)) {
            func_is_public = 1;
        } else if (match_token(parser, TOK_PRIVATE)) {
            func_is_public = 0;
        }

        if (!match_token(parser, TOK_FUNC)) {
            parse_error(parser, "Expected 'func' in extern block");
            return NULL;
        }

        Node* funcDeclNode = parse_func_decl(parser, func_is_public);
        if (!funcDeclNode) {
            return NULL;
        }
        funcDeclNode->as.func_decl.is_extern = 1;
        nodelist_push(&node->as.extern_module.decls, funcDeclNode);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after extern module declarations");
    return node;
}

// ============================================================================
// Import Handling
// ============================================================================

static char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char*  buffer = xmalloc(size + 1);
    size_t read   = fread(buffer, 1, size, file);
    buffer[read]  = '\0';
    fclose(file);
    return buffer;
}

// Check if a module has already been imported
static int is_module_imported(Parser* parser, const char* module_name, size_t length) {
    for (int i = 0; i < parser->imported_modules_count; i++) {
        if (strlen(parser->imported_modules[i]) == length &&
            strncmp(parser->imported_modules[i], module_name, length) == 0) {
            return 1;
        }
    }
    return 0;
}

// Add a module name to the imported list
static void add_imported_module(Parser* parser, const char* module_name, size_t length) {
    VEC_GROW(parser->imported_modules, parser->imported_modules_count,
             parser->imported_modules_capacity);

    // Copy the module name
    char* name_copy = xmalloc(length + 1);
    memcpy(name_copy, module_name, length);
    name_copy[length]                                          = '\0';
    parser->imported_modules[parser->imported_modules_count++] = name_copy;
}

// Add source buffer to parser's list (keeps it alive for AST references)
static void add_imported_source(Parser* parser, char* source) {
    VEC_GROW(parser->imported_sources, parser->imported_sources_count,
             parser->imported_sources_capacity);
    parser->imported_sources[parser->imported_sources_count++] = source;
}

// Add a direct import (library module directly imported by current file)
static void add_direct_import(Parser* parser, const char* module_name, size_t length) {
    VEC_GROW(parser->direct_imports, parser->direct_imports_count, parser->direct_imports_capacity);

    // Copy the module name
    char* name_copy = xmalloc(length + 1);
    memcpy(name_copy, module_name, length);
    name_copy[length]                                      = '\0';
    parser->direct_imports[parser->direct_imports_count++] = name_copy;
}

// Check if file exists
static int file_exists(const char* path) {
    FILE* f = fopen(path, "r");
    if (f) {
        fclose(f);
        return 1;
    }
    return 0;
}

// Check if path is a relative import (starts with ./ or ../)
static int is_relative_path(const char* path, size_t length) {
    if (length >= 2 && path[0] == '.' && path[1] == '/') {
        return 1;
    }
    if (length >= 3 && path[0] == '.' && path[1] == '.' && path[2] == '/') {
        return 1;
    }
    return 0;
}

// Build path to imported module
// For relative paths (./ or ../), resolves relative to source file
// For module names, tries source-relative lib/ first, then falls back to cwd-relative lib/
static void build_import_path(Parser* parser, char* path, size_t path_size, const char* module_name,
                              size_t module_length, int is_relative) {
    if (is_relative) {
        // Relative import: resolve relative to source file's directory
        if (parser->source_path) {
            char* path_copy = xstrdup(parser->source_path);
            char* dir       = dirname(path_copy);
            snprintf(path, path_size, "%s/%.*s", dir, (int)module_length, module_name);
            free(path_copy);
            return;
        }
        // Fallback if no source path: use path as-is relative to cwd
        snprintf(path, path_size, "%.*s", (int)module_length, module_name);
        return;
    }

    // Standard library import: try lib/ directories
    // Highest priority: --lib-path flag
    if (parser->lib_path) {
        snprintf(path, path_size, "%s/%.*s.w", parser->lib_path, (int)module_length, module_name);
        if (file_exists(path)) {
            return;
        }
    }

    if (parser->source_path) {
        // Make a copy because dirname may modify its argument
        char* path_copy = xstrdup(parser->source_path);
        char* dir       = dirname(path_copy);
        snprintf(path, path_size, "%s/lib/%.*s.w", dir, (int)module_length, module_name);
        free(path_copy);
        if (file_exists(path)) {
            return;
        }
        // Fall through to try cwd-relative path
    }
    // Fallback: use lib/ relative to current working directory
    snprintf(path, path_size, "lib/%.*s.w", (int)module_length, module_name);
}

// Parse a use statement: use module.symbol; or use module.{sym1, sym2};
static Node* parse_use_stmt(Parser* parser) {
    Token module_token = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name after 'use'");

    consume_token(parser, TOK_DOT, "Expected '.' after module name in use statement");

    // Allocate arrays for symbol names
    int    capacity     = 4;
    char** symbol_names = xmalloc(capacity * sizeof(char*));
    int*   name_lengths = xmalloc(capacity * sizeof(int));
    int    count        = 0;

    if (match_token(parser, TOK_LBRACE)) {
        // Grouped: use module.{sym1, sym2}
        while (!check_token(parser, TOK_RBRACE) && !check_token(parser, TOK_EOF)) {
            Token sym = parser->current;
            consume_token(parser, TOK_IDENT, "Expected symbol name in use group");
            if (count >= capacity) {
                capacity *= 2;
                symbol_names = xrealloc(symbol_names, capacity * sizeof(char*));
                name_lengths = xrealloc(name_lengths, capacity * sizeof(int));
            }
            symbol_names[count] = copy_token_string(&sym);
            name_lengths[count] = sym.length;
            count++;
            if (!check_token(parser, TOK_RBRACE)) {
                consume_token(parser, TOK_COMMA, "Expected ',' or '}' in use group");
            }
        }
        consume_token(parser, TOK_RBRACE, "Expected '}' after use group");
    } else {
        // Single: use module.symbol
        Token sym = parser->current;
        consume_token(parser, TOK_IDENT, "Expected symbol name after '.'");
        symbol_names[0] = copy_token_string(&sym);
        name_lengths[0] = sym.length;
        count           = 1;
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after use statement");

    Node* node                    = node_new(NODE_USE_DECL, module_token.line, module_token.column);
    node->as.use_decl.module_name = copy_token_string(&module_token);
    node->as.use_decl.module_name_length  = module_token.length;
    node->as.use_decl.symbol_names        = symbol_names;
    node->as.use_decl.symbol_name_lengths = name_lengths;
    node->as.use_decl.symbol_count        = count;
    return node;
}

static int parse_import_stmt(Parser* parser, Node* program, Node* current_module) {
    // Expect identifier or string after 'import'
    Token       import_token = parser->current;
    const char* module_name;
    size_t      module_length;
    int         is_relative = 0;

    if (parser->current.type == TOK_STRING) {
        // String import: "./path/to/file.w" or "../path/to/file.w"
        advance_token(parser);
        // Extract path without quotes
        module_name   = import_token.start + 1;
        module_length = import_token.length - 2;
        is_relative   = is_relative_path(module_name, module_length);
        if (!is_relative) {
            parse_error(parser, "String imports must be relative paths (start with ./ or ../)");
            return 0;
        }
    } else if (parser->current.type == TOK_IDENT) {
        // Identifier import: std
        advance_token(parser);
        module_name   = import_token.start;
        module_length = import_token.length;
    } else {
        parse_error(parser, "Expected module name or path after 'import'");
        return 0;
    }

    // Expect semicolon
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    // Build path to import file first (needed for duplicate detection with relative paths)
    char path[1024];
    build_import_path(parser, path, sizeof(path), module_name, module_length, is_relative);

    // For relative imports, use the resolved path as the module key for duplicate detection
    const char* module_key        = is_relative ? path : module_name;
    size_t      module_key_length = is_relative ? strlen(path) : module_length;

    // Check if already imported
    if (is_module_imported(parser, module_key, module_key_length)) {
        // Module already imported - but still track as direct import if it's a library import
        // This allows this file to access the module's symbols even if a dependency imported it
        // first
        if (!is_relative) {
            add_direct_import(parser, module_name, module_length);
        }
        return 1; // Already imported, nothing more to do
    }

    // Mark as imported before parsing (prevents cycles)
    add_imported_module(parser, module_key, module_key_length);

    // Read the imported file
    char* source = read_file(path);
    if (!source) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Could not read import file: %s", path);
        parse_error(parser, error_msg);
        return 0;
    }

    // Store source buffer to keep it alive (AST nodes reference strings in it)
    add_imported_source(parser, source);

    // Parse the imported file (inherits source_path for nested imports)
    Parser import_parser;
    parser_init_with_path(&import_parser, source, path, parser->lib_path);

    // Share the imported modules list with the sub-parser to handle transitive imports
    import_parser.imported_modules          = parser->imported_modules;
    import_parser.imported_modules_count    = parser->imported_modules_count;
    import_parser.imported_modules_capacity = parser->imported_modules_capacity;

    Node* import_ast = parser_parse(&import_parser);

    // Update parent parser's imported modules list (sub-parser may have added more)
    parser->imported_modules          = import_parser.imported_modules;
    parser->imported_modules_count    = import_parser.imported_modules_count;
    parser->imported_modules_capacity = import_parser.imported_modules_capacity;

    // Move any imported sources from sub-parser to parent
    for (int i = 0; i < import_parser.imported_sources_count; i++) {
        add_imported_source(parser, import_parser.imported_sources[i]);
    }
    // Clear sub-parser's list (don't free, ownership transferred)
    free(import_parser.imported_sources);
    import_parser.imported_sources       = NULL;
    import_parser.imported_sources_count = 0;

    // Clear the imported_modules in sub-parser to prevent double-free
    import_parser.imported_modules       = NULL;
    import_parser.imported_modules_count = 0;

    // Free sub-parser's direct_imports (not needed - each file tracks its own imports via
    // source_file)
    for (int i = 0; i < import_parser.direct_imports_count; i++) {
        free(import_parser.direct_imports[i]);
    }
    free(import_parser.direct_imports);
    import_parser.direct_imports       = NULL;
    import_parser.direct_imports_count = 0;

    if (import_parser.had_error) {
        fprintf(stderr, "Failed to parse imported file: %s\n", path);
        node_free(import_ast);
        return 0;
    }

    // For library imports, track as direct import
    if (!is_relative) {
        add_direct_import(parser, module_name, module_length);
    }

    // Handle the imported AST based on import type
    if (import_ast && import_ast->type == NODE_PROGRAM) {
        if (is_relative) {
            // Relative import: merge only the "main" module's declarations into current_module
            // Keep library modules (non-main) as separate modules in the program
            for (int m = 0; m < import_ast->as.program.modules.count; m++) {
                Node* imported_module = import_ast->as.program.modules.nodes[m];
                if (imported_module && imported_module->type == NODE_MODULE) {
                    if (strcmp(imported_module->as.module.name, "main") == 0) {
                        // Merge main module's declarations into current module
                        for (int i = 0; i < imported_module->as.module.decls.count; i++) {
                            Node* decl = imported_module->as.module.decls.nodes[i];
                            nodelist_push(&current_module->as.module.decls, decl);
                        }
                        // Clear to prevent double-free
                        imported_module->as.module.decls.count = 0;
                    } else {
                        // Keep library modules as separate modules
                        nodelist_push(&program->as.program.modules, imported_module);
                    }
                }
            }
            // Clear to prevent double-free (only main was freed, others moved)
            import_ast->as.program.modules.count = 0;
        } else {
            // Library import: move modules from imported AST to program
            // The first module (main) of the library becomes a module named after the import
            for (int m = 0; m < import_ast->as.program.modules.count; m++) {
                Node* imported_module = import_ast->as.program.modules.nodes[m];
                if (imported_module && imported_module->type == NODE_MODULE) {
                    // Rename "main" module to the library name
                    if (strcmp(imported_module->as.module.name, "main") == 0) {
                        free(imported_module->as.module.name);
                        imported_module->as.module.name = xmalloc(module_length + 1);
                        memcpy(imported_module->as.module.name, module_name, module_length);
                        imported_module->as.module.name[module_length] = '\0';
                        imported_module->as.module.name_length         = (int)module_length;
                    }
                    nodelist_push(&program->as.program.modules, imported_module);
                }
            }
            // Clear to prevent double-free
            import_ast->as.program.modules.count = 0;
        }
    }

    node_free(import_ast);
    return 1;
}

// ============================================================================
// Top-level Declaration Parsing
// ============================================================================

static Node* parse_declaration(Parser* parser) {
    int is_public      = match_token(parser, TOK_PUBLIC);
    int has_visibility = is_public;

    if (!is_public) {
        has_visibility = match_token(parser, TOK_PRIVATE);
        is_public      = !has_visibility; // default to public if no modifier
    }
    if (match_token(parser, TOK_EXTERN)) {
        // Extern defaults to private when no visibility modifier
        return parse_extern_decls(parser, has_visibility ? is_public : 0);
    }
    if (match_token(parser, TOK_FUNC)) {
        return parse_func_decl(parser, is_public);
    }
    if (match_token(parser, TOK_STRUCT)) {
        return parse_struct_decl(parser, is_public);
    }
    if (match_token(parser, TOK_ENUM)) {
        return parse_enum_decl(parser, is_public);
    }
    if (match_token(parser, TOK_TRAIT)) {
        return parse_trait_decl(parser, is_public);
    }
    if (match_token(parser, TOK_TYPE)) {
        return parse_type_alias(parser, is_public);
    }
    if (match_token(parser, TOK_IMPL)) {
        return parse_impl_decl(parser);
    }
    if (match_token(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0, is_public);
    }
    if (match_token(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1, is_public);
    }

    parse_error(parser, "Expected declaration");
    return NULL;
}

// ============================================================================
// Parser Entry Points
// ============================================================================

void parser_init(Parser* parser, const char* source) {
    parser_init_with_path(parser, source, NULL, NULL);
}

void parser_init_with_path(Parser* parser, const char* source, const char* source_path,
                           const char* lib_path) {
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parser->parse_depth  = 0; // Reset recursion depth

    parser->source_path = source_path;
    parser->lib_path    = lib_path;

    // Initialize imported sources tracking
    parser->imported_sources          = NULL;
    parser->imported_sources_count    = 0;
    parser->imported_sources_capacity = 0;

    // Initialize imported modules tracking
    parser->imported_modules          = NULL;
    parser->imported_modules_count    = 0;
    parser->imported_modules_capacity = 0;

    // Initialize direct imports tracking (library modules directly imported by this file)
    parser->direct_imports          = NULL;
    parser->direct_imports_count    = 0;
    parser->direct_imports_capacity = 0;

    advance_token(parser); // Prime the parser
}

void parser_free(Parser* parser) {
    // Free all imported source buffers
    for (int i = 0; i < parser->imported_sources_count; i++) {
        free(parser->imported_sources[i]);
    }
    free(parser->imported_sources);

    // Free all imported module names
    for (int i = 0; i < parser->imported_modules_count; i++) {
        free(parser->imported_modules[i]);
    }
    free(parser->imported_modules);

    // Free all direct import names
    for (int i = 0; i < parser->direct_imports_count; i++) {
        free(parser->direct_imports[i]);
    }
    free(parser->direct_imports);
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    nodelist_init(&program->as.program.modules);

    // Create main module for the entry file
    Node* main_module                  = node_new(NODE_MODULE, 1, 1);
    main_module->as.module.name        = xstrdup("main");
    main_module->as.module.name_length = 4;
    nodelist_init(&main_module->as.module.decls);
    nodelist_push(&program->as.program.modules, main_module);

    while (!check_token(parser, TOK_EOF)) {
        // Handle import statements
        if (match_token(parser, TOK_IMPORT)) {
            if (!parse_import_stmt(parser, program, main_module)) {
                // Import failed, but continue parsing
                if (parser->panic_mode)
                    synchronize(parser);
            }
            continue;
        }

        // Handle use statements
        if (match_token(parser, TOK_USE)) {
            Node* use_node = parse_use_stmt(parser);
            if (use_node) {
                nodelist_push(&main_module->as.module.decls, use_node);
            }
            if (parser->panic_mode)
                synchronize(parser);
            continue;
        }

        Node* decl = parse_declaration(parser);
        if (decl) {
            nodelist_push(&main_module->as.module.decls, decl);
        }
        if (parser->panic_mode)
            synchronize(parser);
    }

    return program;
}
