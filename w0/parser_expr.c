#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "parser_internal.h"

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

static Node* parse_lambda_expr(Parser* parser, Token start) {
    Node* node = node_new(NODE_LAMBDA, start.line, start.column);
    nodelist_init(&node->as.lambda.params);
    node->as.lambda.return_type   = NULL;
    node->as.lambda.body          = NULL;
    node->as.lambda.is_expr_body  = 0;
    node->as.lambda.lambda_id     = 0;
    node->as.lambda.resolved_type = NULL;

    // Parse params: |x: T, y: U| or ||
    if (!check_token(parser, TOK_PIPE)) {
        for (;;) {
            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected parameter name in lambda");

            Node* param                 = node_new(NODE_PARAM, param_name.line, param_name.column);
            param->as.param.name        = copy_token_string(&param_name);
            param->as.param.name_length = param_name.length;
            param->as.param.is_const    = 0;

            consume_token(parser, TOK_COLON, "Expected ':' after lambda parameter name");
            param->as.param.type = parse_type(parser);

            nodelist_push(&node->as.lambda.params, param);

            if (!match_token(parser, TOK_COMMA))
                break;
        }
    }
    consume_token(parser, TOK_PIPE, "Expected '|' after lambda parameters");

    // Optional return type: -> Type
    if (match_token(parser, TOK_ARROW)) {
        node->as.lambda.return_type = parse_type(parser);
    }

    // Body: block or expression
    if (match_token(parser, TOK_LBRACE)) {
        node->as.lambda.body         = parse_block(parser);
        node->as.lambda.is_expr_body = 0;
    } else {
        node->as.lambda.body         = parse_expression(parser);
        node->as.lambda.is_expr_body = 1;
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
    case TOK_PIPE:
        advance_token(parser);
        return parse_lambda_expr(parser, token);
    case TOK_PIPE_PIPE: {
        // || is lexed as a single token; treat as empty-param lambda
        advance_token(parser);
        Node* lam = node_new(NODE_LAMBDA, token.line, token.column);
        nodelist_init(&lam->as.lambda.params);
        lam->as.lambda.return_type   = NULL;
        lam->as.lambda.lambda_id     = 0;
        lam->as.lambda.resolved_type = NULL;
        if (match_token(parser, TOK_ARROW)) {
            lam->as.lambda.return_type = parse_type(parser);
        }
        if (match_token(parser, TOK_LBRACE)) {
            lam->as.lambda.body         = parse_block(parser);
            lam->as.lambda.is_expr_body = 0;
        } else {
            lam->as.lambda.body         = parse_expression(parser);
            lam->as.lambda.is_expr_body = 1;
        }
        return lam;
    }
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

Node* parse_expression(Parser* parser) {
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
