#include "parser.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

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
static Node*            parse_match(Parser* parser, int is_expr);
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

    // Function type: func(T1, T2): ReturnType
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

        if (match_token(parser, TOK_COLON)) {
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
            buf[buf_len++] = decode_escape(*src);
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

static Node* parse_int_lit(Parser* parser, Token token) {
    Node*       node  = node_new(NODE_INT_LIT, token.line, token.column);
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

static Node* parse_float_lit(Token token) {
    Node* node               = node_new(NODE_FLOAT_LIT, token.line, token.column);
    node->as.float_lit.value = strtod(token.start, NULL);
    return node;
}

static Node* parse_string_lit(Token token) {
    Node* node = node_new(NODE_STRING_LIT, token.line, token.column);

    int is_triple = token.length >= 6 && token.start[0] == '"' && token.start[1] == '"' &&
                    token.start[2] == '"';

    const char* src;
    const char* end;
    if (is_triple) {
        src = token.start + 3;
        end = token.start + token.length - 3;
    } else {
        src = token.start + 1;
        end = token.start + token.length - 1;
    }

    // Phase 1: Decode escape sequences into a raw buffer
    size_t max_len = end - src;
    char*  raw     = xmalloc(max_len + 1);
    size_t raw_len = 0;
    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            raw[raw_len++] = decode_escape(*src++);
        } else {
            raw[raw_len++] = *src++;
        }
    }
    raw[raw_len] = '\0';

    if (!is_triple) {
        node->as.string_lit.value  = raw;
        node->as.string_lit.length = raw_len;
        return node;
    }

    // Phase 2: Triple-quoted indentation stripping
    // Skip leading newline after opening """
    const char* content     = raw;
    size_t      content_len = raw_len;
    if (content_len > 0 && content[0] == '\n') {
        content++;
        content_len--;
    }

    // Find the last line (after final newline) to determine indent prefix
    const char* last_newline = NULL;
    for (size_t i = content_len; i > 0; i--) {
        if (content[i - 1] == '\n') {
            last_newline = &content[i - 1];
            break;
        }
    }

    size_t      prefix_len = 0;
    const char* prefix     = NULL;
    if (last_newline) {
        // Last line is everything after the final newline
        const char* last_line     = last_newline + 1;
        size_t      last_line_len = content_len - (last_line - content);
        // Check if last line is all whitespace
        int all_ws = 1;
        for (size_t i = 0; i < last_line_len; i++) {
            if (last_line[i] != ' ' && last_line[i] != '\t') {
                all_ws = 0;
                break;
            }
        }
        if (all_ws) {
            prefix     = last_line;
            prefix_len = last_line_len;
            // Remove the trailing whitespace-only line (and its preceding newline)
            content_len = last_newline - content;
        }
    }

    // Build result by stripping prefix from start of each line
    char*  result     = xmalloc(content_len + 1);
    size_t result_len = 0;
    size_t i          = 0;
    while (i < content_len) {
        // Strip prefix at start of line
        if (prefix_len > 0 && i + prefix_len <= content_len &&
            memcmp(&content[i], prefix, prefix_len) == 0) {
            i += prefix_len;
        }
        // Copy until end of line
        while (i < content_len && content[i] != '\n') {
            result[result_len++] = content[i++];
        }
        // Copy the newline
        if (i < content_len && content[i] == '\n') {
            result[result_len++] = content[i++];
        }
    }
    result[result_len] = '\0';

    free(raw);
    node->as.string_lit.value  = result;
    node->as.string_lit.length = result_len;
    return node;
}

static Node* parse_char_lit(Token token) {
    Node* node = node_new(NODE_CHAR_LIT, token.line, token.column);
    if (token.start[1] == '\\') {
        node->as.char_lit.value = decode_escape(token.start[2]);
    } else {
        node->as.char_lit.value = token.start[1];
    }
    return node;
}

static Node* parse_bool_lit(Token token) {
    Node* node              = node_new(NODE_BOOL_LIT, token.line, token.column);
    node->as.bool_lit.value = (token.type == TOK_TRUE);
    return node;
}

static Node* parse_null_lit(Token token) {
    return node_new(NODE_NULL_LIT, token.line, token.column);
}

static Node* parse_ident_lit(Token token) {
    Node* node            = node_new(NODE_IDENT, token.line, token.column);
    node->as.ident.name   = copy_token_string(&token);
    node->as.ident.length = token.length;
    return node;
}

static Node* parse_enum_value(Parser* parser, Token enum_name) {
    advance_token(parser); // consume ::
    Token value_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected enum value name after '::'");

    Node* node                    = node_new(NODE_ENUM_VALUE, enum_name.line, enum_name.column);
    node->as.enum_value.enum_name = copy_token_string(&enum_name);
    node->as.enum_value.enum_name_length  = enum_name.length;
    node->as.enum_value.value_name        = copy_token_string(&value_name);
    node->as.enum_value.value_name_length = value_name.length;
    nodelist_init(&node->as.enum_value.args);

    if (match_token(parser, TOK_LPAREN)) {
        while (!check_token(parser, TOK_RPAREN) && !check_token(parser, TOK_EOF)) {
            Node* arg = parse_expression(parser);
            if (!arg)
                return NULL;
            nodelist_push(&node->as.enum_value.args, arg);
            if (!check_token(parser, TOK_RPAREN)) {
                consume_token(parser, TOK_COMMA, "Expected ',' or ')' after enum constructor arg");
            }
        }
        consume_token(parser, TOK_RPAREN, "Expected ')' after enum constructor args");
    }
    return node;
}

static Node* parse_ident_or_enum_value(Parser* parser, Token token) {
    if (check_token(parser, TOK_COLON_COLON)) {
        return parse_enum_value(parser, token);
    }
    return parse_ident_lit(token);
}

static Node* parse_self_ident(Token token) {
    Node* node            = node_new(NODE_IDENT, token.line, token.column);
    node->as.ident.name   = xstrdup("self");
    node->as.ident.length = 4;
    return node;
}

static Node* parse_group_or_tuple(Parser* parser, Token token) {
    Node* first = parse_expression(parser);
    if (!first) {
        return NULL;
    }

    if (match_token(parser, TOK_COMMA)) {
        Node* node = node_new(NODE_TUPLE_LIT, token.line, token.column);
        nodelist_init(&node->as.tuple_lit.elements);
        nodelist_push(&node->as.tuple_lit.elements, first);

        Node* elem = parse_expression(parser);
        if (!elem) {
            node_free(node);
            return NULL;
        }
        nodelist_push(&node->as.tuple_lit.elements, elem);

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

    consume_token(parser, TOK_RPAREN, "Expected ')' after expression");
    return first;
}

static Node* parse_new_expr(Parser* parser, Token new_token) {
    Node* type_node = parse_type(parser);
    if (!type_node) {
        return NULL;
    }

    Node* node                      = node_new(NODE_NEW_EXPR, new_token.line, new_token.column);
    node->as.new_expr.type_node     = type_node;
    node->as.new_expr.resolved_type = NULL;

    if (check_token(parser, TOK_LPAREN)) {
        // Init-call form: new Type(args)
        advance_token(parser); // consume '('
        node->as.new_expr.init = NULL;
        nodelist_init(&node->as.new_expr.args);
        if (!check_token(parser, TOK_RPAREN)) {
            do {
                Node* arg = parse_expression(parser);
                if (!arg) {
                    node_free(node);
                    return NULL;
                }
                nodelist_push(&node->as.new_expr.args, arg);
            } while (match_token(parser, TOK_COMMA));
        }
        consume_token(parser, TOK_RPAREN, "Expected ')' after init arguments");
    } else {
        // Struct literal form: new Type { fields }
        consume_token(parser, TOK_LBRACE, "Expected '{' or '(' after type in new expression");
        Node* init = parse_struct_init(parser);
        if (!init) {
            node_free(node);
            return NULL;
        }
        node->as.new_expr.init = init;
        nodelist_init(&node->as.new_expr.args);
    }
    return node;
}

static Node* parse_array_lit(Parser* parser, Token token) {
    Node* node = node_new(NODE_ARRAY_LIT, token.line, token.column);
    nodelist_init(&node->as.array_lit.elements);
    node->as.array_lit.resolved_type = NULL;

    if (!check_token(parser, TOK_RBRACKET)) {
        Node* elem = parse_expression(parser);
        if (!elem) {
            node_free(node);
            return NULL;
        }
        nodelist_push(&node->as.array_lit.elements, elem);

        while (match_token(parser, TOK_COMMA)) {
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

static Node* parse_primary_expression(Parser* parser) {
    Token token = parser->current;

    switch (token.type) {
    case TOK_INT:
        advance_token(parser);
        return parse_int_lit(parser, token);
    case TOK_FLOAT:
        advance_token(parser);
        return parse_float_lit(token);
    case TOK_INTERP_STRING:
        advance_token(parser);
        return parse_interp_string(parser);
    case TOK_STRING:
        advance_token(parser);
        return parse_string_lit(token);
    case TOK_CHAR:
        advance_token(parser);
        return parse_char_lit(token);
    case TOK_TRUE:
    case TOK_FALSE:
        advance_token(parser);
        return parse_bool_lit(token);
    case TOK_NULL:
        advance_token(parser);
        return parse_null_lit(token);
    case TOK_MATCH:
        advance_token(parser);
        return parse_match(parser, 1);
    case TOK_IDENT:
        advance_token(parser);
        return parse_ident_or_enum_value(parser, token);
    case TOK_SELF:
        advance_token(parser);
        return parse_self_ident(token);
    case TOK_LPAREN:
        advance_token(parser);
        return parse_group_or_tuple(parser, token);
    case TOK_NEW:
        advance_token(parser);
        return parse_new_expr(parser, token);
    case TOK_LBRACKET:
        advance_token(parser);
        return parse_array_lit(parser, token);
    default:
        parse_error(parser, "Expected expression");
        return NULL;
    }
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
        } else if (match_token(parser, TOK_QUESTION)) {
            // Try expression: expr?
            Node* try_node             = node_new(NODE_TRY_EXPR, expr->line, expr->column);
            try_node->as.try_expr.expr = expr;
            expr                       = try_node;
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

static Node* parse_match(Parser* parser, int is_expr) {
    Token token = parser->previous; // TOK_MATCH already consumed
    consume_token(parser, TOK_LPAREN, "Expected '(' after 'match'");
    Node* expr = parse_expression(parser);
    consume_token(parser, TOK_RPAREN, "Expected ')' after match expression");
    consume_token(parser, TOK_LBRACE, "Expected '{' after match expression");

    Node* node                              = node_new(NODE_MATCH, token.line, token.column);
    node->as.match_stmt.expr                = expr;
    node->as.match_stmt.resolved_type       = NULL;
    node->as.match_stmt.resolved_value_type = NULL;
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

        if (is_expr) {
            if (check_token(parser, TOK_LBRACE)) {
                parse_error(parser, "Match expression arms must be expressions (no block body)");
                node_free(arm);
                node_free(node);
                return NULL;
            }
            arm->as.match_arm.body = parse_expression(parser);
            if (!arm->as.match_arm.body) {
                node_free(arm);
                node_free(node);
                return NULL;
            }
        } else {
            // Parse arm body: block or single statement
            if (check_token(parser, TOK_LBRACE)) {
                advance_token(parser);
                arm->as.match_arm.body = parse_block(parser);
            } else {
                arm->as.match_arm.body = parse_statement(parser);
            }
        }

        nodelist_push(&node->as.match_stmt.arms, arm);

        // Optional comma between arms
        match_token(parser, TOK_COMMA);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after match arms");
    return node;
}

static Node* parse_match_stmt(Parser* parser) {
    return parse_match(parser, 0);
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

    // Check for struct destructuring: var {field1, field2} = expr;
    if (check_token(parser, TOK_LBRACE)) {
        advance_token(parser); // consume '{'

        DestructPattern* pattern = pattern_new_struct(4);

        // Parse first field name (required)
        Token field = parser->current;
        consume_token(parser, TOK_IDENT, "Expected field name");
        pattern_struct_push(pattern, field.start, field.length);

        // Parse remaining comma-separated field names
        while (match_token(parser, TOK_COMMA)) {
            field = parser->current;
            consume_token(parser, TOK_IDENT, "Expected field name");
            pattern_struct_push(pattern, field.start, field.length);
        }

        consume_token(parser, TOK_RBRACE, "Expected '}'");

        Node*          node = node_new(NODE_VAR_DECL, start.line, start.column);
        var_decl_node* vdn  = &node->as.var_decl;

        vdn->name             = NULL;
        vdn->name_length      = 0;
        vdn->is_public        = is_public;
        vdn->is_const         = is_const;
        vdn->type             = NULL;
        vdn->init             = NULL;
        vdn->destruct_pattern = pattern;

        // Initializer is required for struct destructuring
        if (!match_token(parser, TOK_EQ)) {
            parse_error(parser, "Struct destructuring requires an initializer");
            node_free(node);
            return NULL;
        }
        vdn->init = parse_expression(parser);

        consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return node;
    }

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
    fdn->type_params        = NULL;
    fdn->type_param_bounds  = NULL;
    fdn->type_param_count   = 0;
    fdn->extern_name        = NULL;
    fdn->extern_name_length = 0;
    fdn->is_varargs         = 0;
    fdn->return_is_const    = 0;
    nodelist_init(&fdn->params);

    // Parse type parameters for generic free functions: func identity<T>(x: T): T
    // Only for free functions (not methods with receivers)
    if (!receiver_type && match_token(parser, TOK_LT)) {
        int capacity           = 4;
        fdn->type_params       = xmalloc(capacity * sizeof(char*));
        fdn->type_param_bounds = xmalloc(capacity * sizeof(char*));

        do {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected type parameter name");

            if (fdn->type_param_count >= capacity) {
                capacity *= 2;
                fdn->type_params       = xrealloc(fdn->type_params, capacity * sizeof(char*));
                fdn->type_param_bounds = xrealloc(fdn->type_param_bounds, capacity * sizeof(char*));
            }

            fdn->type_params[fdn->type_param_count] = copy_token_string(&param_name);

            // Check for trait bound: T: TraitName
            if (match_token(parser, TOK_COLON)) {
                Token bound_name = parser->current;
                consume_token(parser, TOK_IDENT, "Expected trait name after ':'");
                fdn->type_param_bounds[fdn->type_param_count] = copy_token_string(&bound_name);
            } else {
                fdn->type_param_bounds[fdn->type_param_count] = NULL;
            }

            fdn->type_param_count++;
        } while (match_token(parser, TOK_COMMA));

        consume_token(parser, TOK_GT, "Expected '>' after type parameters");
    }

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
        if (match_token(parser, TOK_CONST)) {
            fdn->return_is_const = 1;
        }
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

    // Copy current file's imports to accessible_modules
    // This determines which library modules this function can access
    ModuleLoader* loader          = parser->loader;
    int           fi_count        = loader ? loader->file_imports_count : 0;
    fdn->accessible_modules_count = fi_count;
    if (fi_count > 0) {
        fdn->accessible_modules = xmalloc(fi_count * sizeof(char*));
        for (int i = 0; i < fi_count; i++) {
            fdn->accessible_modules[i] = xstrdup(loader->file_imports[i]);
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
        int field_is_const = 0;
        if (match_token(parser, TOK_CONST)) {
            field_is_const = 1;
        }

        Token field_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected field name");

        Node* field                 = node_new(NODE_FIELD, field_name.line, field_name.column);
        field->as.field.name        = copy_token_string(&field_name);
        field->as.field.name_length = field_name.length;
        field->as.field.is_const    = field_is_const;

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
    Token first_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected name after 'impl'");

    Node* node = node_new(NODE_IMPL_DECL, first_name.line, first_name.column);
    nodelist_init(&node->as.impl_decl.type_args);
    nodelist_init(&node->as.impl_decl.methods);

    if (check_token(parser, TOK_FOR)) {
        // Trait impl: impl Trait for Type { ... }
        advance_token(parser); // consume 'for'
        Token type_name = parser->current;
        consume_token(parser, TOK_IDENT, "Expected type name after 'for'");
        node->as.impl_decl.trait_name        = copy_token_string(&first_name);
        node->as.impl_decl.trait_name_length = first_name.length;
        node->as.impl_decl.type_name         = copy_token_string(&type_name);
        node->as.impl_decl.type_name_length  = type_name.length;
    } else {
        // Inherent impl: impl Type { ... } or impl Type<T> { ... }
        node->as.impl_decl.trait_name        = NULL;
        node->as.impl_decl.trait_name_length = 0;
        node->as.impl_decl.type_name         = copy_token_string(&first_name);
        node->as.impl_decl.type_name_length  = first_name.length;
    }

    // Parse optional type args: impl Drop for Box<T> { ... } or impl Box<T> { ... }
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
        if (check_token(parser, TOK_CONST)) {
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
        value->as.enum_variant.has_explicit_value = 0;
        value->as.enum_variant.explicit_value     = 0;

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
        // Optional explicit integer value: VariantName = 43
        if (match_token(parser, TOK_EQ)) {
            int   is_negative = match_token(parser, TOK_MINUS);
            Token value_token = parser->current;
            consume_token(parser, TOK_INT, "Expected integer literal after '=' in enum variant");
            if (value_token.type != TOK_INT)
                return NULL;

            const char* start = value_token.start;
            int         base  = 10;
            if (value_token.length > 2) {
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
            long  explicit_value = strtol(start, &endptr, base);
            if (errno == ERANGE) {
                parse_error_at(parser, &value_token, "Enum variant value out of range");
            }
            if (is_negative) {
                explicit_value = -explicit_value;
            }
            value->as.enum_variant.has_explicit_value = 1;
            value->as.enum_variant.explicit_value     = explicit_value;
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

        Node* func_node = parse_func_decl(parser, func_is_public);
        if (!func_node) {
            return NULL;
        }
        func_node->as.func_decl.is_extern = 1;
        nodelist_push(&node->as.extern_module.decls, func_node);
    }

    consume_token(parser, TOK_RBRACE, "Expected '}' after extern module declarations");
    return node;
}

// ============================================================================
// Import Handling
// ============================================================================

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
    Token       import_token = parser->current;
    const char* module_name;
    size_t      module_length;
    int         is_relative = 0;

    if (parser->current.type == TOK_STRING) {
        advance_token(parser);
        module_name   = import_token.start + 1;
        module_length = import_token.length - 2;
        is_relative   = module_loader_is_relative_path(module_name, module_length);
        if (!is_relative) {
            parse_error(parser, "String imports must be relative paths (start with ./ or ../)");
            return 0;
        }
    } else if (parser->current.type == TOK_IDENT) {
        advance_token(parser);
        module_name   = import_token.start;
        module_length = import_token.length;
    } else {
        parse_error(parser, "Expected module name or path after 'import'");
        return 0;
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after import statement");

    return module_loader_import(parser->loader, parser, program, current_module, module_name,
                                module_length, is_relative);
}

// ============================================================================
// Test Declaration Parsing
// ============================================================================

static Node* parse_test_decl(Parser* parser) {
    int line = parser->previous.line;
    int col  = parser->previous.column;

    if (parser->current.type != TOK_STRING) {
        parse_error(parser, "Expected test name string after 'test'");
        return NULL;
    }
    advance_token(parser);

    // Extract the string contents (without quotes)
    const char* name_start  = parser->previous.start + 1;       // skip opening quote
    int         name_length = (int)parser->previous.length - 2; // exclude both quotes

    Node* node              = node_new(NODE_TEST_DECL, line, col);
    node->as.test_decl.name = xmalloc(name_length + 1);
    memcpy(node->as.test_decl.name, name_start, name_length);
    node->as.test_decl.name[name_length] = '\0';
    node->as.test_decl.name_length       = name_length;

    consume_token(parser, TOK_LBRACE, "Expected '{' after test name");
    node->as.test_decl.body = parse_block(parser);

    return node;
}

// ============================================================================
// Top-level Declaration Parsing
// ============================================================================

static Node* parse_declaration(Parser* parser) {
    // test blocks have no visibility modifier
    if (check_token(parser, TOK_TEST)) {
        advance_token(parser);
        return parse_test_decl(parser);
    }

    int is_public      = match_token(parser, TOK_PUBLIC);
    int has_visibility = is_public;

    if (!is_public) {
        has_visibility = match_token(parser, TOK_PRIVATE);
        is_public      = !has_visibility; // default to public if no modifier
    }

    // Error if visibility modifier used with test
    if (has_visibility && check_token(parser, TOK_TEST)) {
        parse_error(parser, "Test blocks cannot have visibility modifiers");
        return NULL;
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
    parser_init_with_loader(parser, source, NULL, NULL);
}

void parser_init_with_loader(Parser* parser, const char* source, const char* source_path,
                             ModuleLoader* loader) {
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parser->parse_depth  = 0;

    parser->source_path = source_path;
    parser->loader      = loader;

    advance_token(parser);
}

void parser_free(Parser* parser) {
    (void)parser;
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    nodelist_init(&program->as.program.modules);

    // Create main module for the entry file
    Node* main_module                  = node_new(NODE_MODULE, 1, 1);
    main_module->as.module.name        = xstrdup("main");
    main_module->as.module.name_length = 4;
    nodelist_init(&main_module->as.module.decls);

    // Auto-import prelude before adding main module, so prelude types
    // are defined first in the checker (main module can then shadow them)
    if (parser->loader) {
        module_loader_import_prelude(parser->loader, parser, program, main_module);
    }

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
