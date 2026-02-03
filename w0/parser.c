#include "parser.h"

#include <errno.h>
#include <libgen.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ============================================================================
// Parser Utilities
// ============================================================================

// Current recursion depth for expression parsing
int parse_depth = 0;

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
        case TOK_VAR:
        case TOK_CONST:
        case TOK_IF:
        case TOK_WHILE:
        case TOK_FOR:
        case TOK_RETURN:
            return;
        default:
            break;
        }
        advance_token(parser);
    }
}

char* copy_token_string(Token* token) {
    char* str = malloc(token->length + 1);
    if (!str) {
        return NULL;
    }
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
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
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
    if (match_token(parser, TOK_IDENT)) {
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

// ============================================================================
// Struct Initializer Parsing
// ============================================================================

static Node* parse_struct_init(Parser* parser) {
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

// ============================================================================
// Primary Expression Parsing
// ============================================================================

static Node* parse_primary_expression(Parser* parser) {
    Token token = parser->current;

    if (match_token(parser, TOK_INT)) {
        Node* node = node_new(NODE_INT_LIT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
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
            parse_error(parser, "Integer literal out of range");
        }
        node->as.int_lit.value = value;
        return node;
    }

    if (match_token(parser, TOK_FLOAT)) {
        Node* node = node_new(NODE_FLOAT_LIT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        node->as.float_lit.value = strtod(token.start, NULL);
        return node;
    }

    if (match_token(parser, TOK_STRING)) {
        Node* node = node_new(NODE_STRING_LIT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        // Allocate max possible size (actual size may be smaller due to escapes)
        size_t max_len            = token.length - 2; // Skip quotes
        node->as.string_lit.value = malloc(max_len + 1);
        if (!node->as.string_lit.value) {
            parse_error(parser, "Out of memory");
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

    if (match_token(parser, TOK_CHAR)) {
        Node* node = node_new(NODE_CHAR_LIT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
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

    if (match_token(parser, TOK_TRUE) || match_token(parser, TOK_FALSE)) {
        Node* node = node_new(NODE_BOOL_LIT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        node->as.bool_lit.value = (token.type == TOK_TRUE);
        return node;
    }

    if (match_token(parser, TOK_NULL)) {
        Node* node = node_new(NODE_NULL_LIT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
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
            if (!node) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            node->as.enum_value.enum_name = copy_token_string(&enum_name);
            if (!node->as.enum_value.enum_name) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            node->as.enum_value.enum_name_length = enum_name.length;
            node->as.enum_value.value_name       = copy_token_string(&value_name);
            if (!node->as.enum_value.value_name) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            node->as.enum_value.value_name_length = value_name.length;
            return node;
        }

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

    // Handle 'self' keyword as an identifier
    if (match_token(parser, TOK_SELF)) {
        Node* node = node_new(NODE_IDENT, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        node->as.ident.name = strdup("self");
        if (!node->as.ident.name) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
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
            if (!node) {
                parse_error(parser, "Out of memory");
                node_free(first);
                return NULL;
            }
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

    if (match_token(parser, TOK_LBRACE)) {
        return parse_struct_init(parser);
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
            Node* call = node_new(NODE_CALL, expr->line, expr->column);
            if (!call) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
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
            // Index
            Node* index = node_new(NODE_INDEX, expr->line, expr->column);
            if (!index) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            index->as.index.object = expr;
            index->as.index.index  = parse_expression(parser);
            consume_token(parser, TOK_RBRACKET, "Expected ']' after index");
            expr = index;
        } else if (match_token(parser, TOK_DOT)) {
            // Member access
            Token name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected member name after '.'");
            Node* member = node_new(NODE_MEMBER, expr->line, expr->column);
            if (!member) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            member->as.member.object = expr;
            member->as.member.name   = copy_token_string(&name);
            if (!member->as.member.name) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
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
        Token op      = parser->previous;
        Node* operand = parse_unary(parser);
        Node* node    = node_new(NODE_UNARY, op.line, op.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
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
    parse_depth++;
    if (parse_depth > MAX_PARSE_DEPTH) {
        parse_error(parser, "Maximum expression nesting depth exceeded");
        parse_depth--;
        return NULL;
    }

    Node* left = parse_unary(parser);
    if (!left) {
        parse_depth--;
        return NULL;
    }

    while (get_precedence(parser->current.type) >= min_prec &&
           get_precedence(parser->current.type) != PREC_NONE) {
        Token op = parser->current;
        advance_token(parser);
        Precedence prec  = get_precedence(op.type);
        Node*      right = parse_binary(parser, prec + 1);

        Node* binary = node_new(NODE_BINARY, op.line, op.column);
        if (!binary) {
            parse_error(parser, "Out of memory");
            parse_depth--;
            return NULL;
        }
        binary->as.binary.op    = op.type;
        binary->as.binary.left  = left;
        binary->as.binary.right = right;
        left                    = binary;
    }

    parse_depth--;
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

        Node* assign = node_new(NODE_ASSIGN, op.line, op.column);
        if (!assign) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
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
    if (!block) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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

    Node* node = node_new(NODE_IF, token.line, token.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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

    Node* node = node_new(NODE_WHILE, token.line, token.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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

    Node* node = node_new(NODE_FOR, token.line, token.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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

    // Parse: start expression
    Node* start = parse_expression(parser);

    // Parse: ..
    consume_token(parser, TOK_DOT_DOT, "Expected '..' in range expression");

    // Parse: end expression
    Node* end  = parse_expression(parser);
    Node* step = NULL;

    if (match_token(parser, TOK_BY)) {
        step = parse_expression(parser);
    } else {
        // Default step is 1
        step = node_new(NODE_INT_LIT, token.line, token.column);
        if (!step) {
            free(var_name);
            parse_error(parser, "Out of memory");
            return NULL;
        }
        step->as.int_lit.value = 1;
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after foreach clauses");
    consume_token(parser, TOK_LBRACE, "Expected '{' after foreach clauses");
    Node* body = parse_block(parser);

    Node* node = node_new(NODE_FOREACH, token.line, token.column);
    if (!node) {
        free(var_name);
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.foreach_stmt.var_name        = var_name;
    node->as.foreach_stmt.var_name_length = var_token.length;
    node->as.foreach_stmt.start           = start;
    node->as.foreach_stmt.end             = end;
    node->as.foreach_stmt.step            = step;
    node->as.foreach_stmt.body            = body;
    return node;
}

static Node* parse_return_stmt(Parser* parser) {
    Token token = parser->previous;
    Node* value = NULL;

    if (!check_token(parser, TOK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after return value");

    Node* node = node_new(NODE_RETURN, token.line, token.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.return_stmt.value = value;
    return node;
}

static Node* parse_defer_stmt(Parser* parser) {
    Token token = parser->previous;

    // Parse the deferred statement (typically an expression statement like a function call)
    Node* stmt = parse_statement(parser);

    Node* node = node_new(NODE_DEFER, token.line, token.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.defer_stmt.stmt = stmt;
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
    if (match_token(parser, TOK_BREAK)) {
        Token token = parser->previous;
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after 'break'");
        Node* node = node_new(NODE_BREAK, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        return node;
    }
    if (match_token(parser, TOK_CONTINUE)) {
        Token token = parser->previous;
        consume_token(parser, TOK_SEMICOLON, "Expected ';' after 'continue'");
        Node* node = node_new(NODE_CONTINUE, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        return node;
    }
    if (match_token(parser, TOK_LBRACE)) {
        return parse_block(parser);
    }

    // Expression statement
    Node* expr = parse_expression(parser);
    consume_token(parser, TOK_SEMICOLON, "Expected ';' after expression");

    Node* node = node_new(NODE_EXPR_STMT, expr ? expr->line : 0, expr ? expr->column : 0);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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
                    realloc(pattern->as.tuple.elements, new_cap * sizeof(DestructPattern*));
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

        Node* node = node_new(NODE_VAR_DECL, start.line, start.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            pattern_free(pattern);
            return NULL;
        }
        var_decl_node* vdn = &node->as.var_decl;

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

    Node* node = node_new(NODE_VAR_DECL, name.line, name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    var_decl_node* vdn = &node->as.var_decl;

    vdn->name = copy_token_string(&name);
    if (!vdn->name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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
    // Check for method receiver: func (Type) or func (const Type)
    char* receiver_type     = NULL;
    int   receiver_type_len = 0;
    int   receiver_is_const = 0;

    if (check_token(parser, TOK_LPAREN)) {
        advance_token(parser); // consume '('

        // Check for 'const' modifier
        if (match_token(parser, TOK_CONST)) {
            receiver_is_const = 1;
        }

        // Expect struct type name
        Token recv_type = parser->current;
        consume_token(parser, TOK_IDENT, "Expected receiver type name");
        receiver_type = copy_token_string(&recv_type);
        if (!receiver_type) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        receiver_type_len = recv_type.length;

        consume_token(parser, TOK_RPAREN, "Expected ')' after receiver type");
    }

    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected function name");

    Node* node = node_new(NODE_FUNC_DECL, name.line, name.column);
    if (!node) {
        free(receiver_type);
        parse_error(parser, "Out of memory");
        return NULL;
    }
    func_decl_node* fdn = &node->as.func_decl;

    fdn->is_public         = is_public;
    fdn->is_extern         = 0;
    fdn->receiver_type     = receiver_type;
    fdn->receiver_type_len = receiver_type_len;
    fdn->receiver_is_const = receiver_is_const;
    fdn->name              = copy_token_string(&name);
    if (!fdn->name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    fdn->name_length = name.length;
    nodelist_init(&fdn->params);

    consume_token(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check_token(parser, TOK_RPAREN)) {
        do {
            // Check for 'const' modifier
            int param_is_const = 0;
            if (match_token(parser, TOK_CONST)) {
                param_is_const = 1;
            }

            Token param_name = parser->current;
            consume_token(parser, TOK_IDENT, "Expected parameter name");

            Node* param = node_new(NODE_PARAM, param_name.line, param_name.column);
            if (!param) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            param->as.param.name = copy_token_string(&param_name);
            if (!param->as.param.name) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            param->as.param.name_length = param_name.length;
            param->as.param.type        = NULL;
            param->as.param.is_const    = param_is_const;

            if (match_token(parser, TOK_COLON)) {
                param->as.param.type = parse_type(parser);
            }

            nodelist_push(&fdn->params, param);
        } while (match_token(parser, TOK_COMMA));
    }

    consume_token(parser, TOK_RPAREN, "Expected ')' after parameters");

    // Return type
    fdn->return_type = NULL;
    if (match_token(parser, TOK_COLON)) {
        fdn->return_type = parse_type(parser);
    }

    // Body
    if (check_token(parser, TOK_LBRACE) == 0) {
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
        fdn->accessible_modules = malloc(parser->direct_imports_count * sizeof(char*));
        if (fdn->accessible_modules) {
            for (int i = 0; i < parser->direct_imports_count; i++) {
                fdn->accessible_modules[i] = strdup(parser->direct_imports[i]);
            }
        }
    } else {
        fdn->accessible_modules = NULL;
    }

    return node;
}

static Node* parse_struct_decl(Parser* parser, int is_public) {
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

static Node* parse_enum_decl(Parser* parser, int is_public) {
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

static Node* parse_extern_decls(Parser* parser, int is_public) {
    Token module_name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected module name string after 'extern'");

    Node* node = node_new(NODE_EXTERN_MODULE, module_name.line, module_name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.extern_module.module_name = copy_token_string(&module_name);
    if (!node->as.extern_module.module_name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
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

    char* buffer = malloc(size + 1);
    if (!buffer) {
        fclose(file);
        return NULL;
    }

    size_t read  = fread(buffer, 1, size, file);
    buffer[read] = '\0';
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
    // Grow array if needed
    if (parser->imported_modules_count >= parser->imported_modules_capacity) {
        int new_capacity =
            parser->imported_modules_capacity == 0 ? 8 : parser->imported_modules_capacity * 2;
        char** new_array = realloc(parser->imported_modules, new_capacity * sizeof(char*));
        if (!new_array)
            return;
        parser->imported_modules          = new_array;
        parser->imported_modules_capacity = new_capacity;
    }

    // Copy the module name
    char* name_copy = malloc(length + 1);
    if (!name_copy)
        return;
    memcpy(name_copy, module_name, length);
    name_copy[length]                                          = '\0';
    parser->imported_modules[parser->imported_modules_count++] = name_copy;
}

// Add source buffer to parser's list (keeps it alive for AST references)
static void add_imported_source(Parser* parser, char* source) {
    // Grow array if needed
    if (parser->imported_sources_count >= parser->imported_sources_capacity) {
        int new_capacity =
            parser->imported_sources_capacity == 0 ? 8 : parser->imported_sources_capacity * 2;
        char** new_array = realloc(parser->imported_sources, new_capacity * sizeof(char*));
        if (!new_array)
            return;
        parser->imported_sources          = new_array;
        parser->imported_sources_capacity = new_capacity;
    }
    parser->imported_sources[parser->imported_sources_count++] = source;
}

// Add a direct import (library module directly imported by current file)
static void add_direct_import(Parser* parser, const char* module_name, size_t length) {
    // Grow array if needed
    if (parser->direct_imports_count >= parser->direct_imports_capacity) {
        int new_capacity =
            parser->direct_imports_capacity == 0 ? 8 : parser->direct_imports_capacity * 2;
        char** new_array = realloc(parser->direct_imports, new_capacity * sizeof(char*));
        if (!new_array)
            return;
        parser->direct_imports          = new_array;
        parser->direct_imports_capacity = new_capacity;
    }

    // Copy the module name
    char* name_copy = malloc(length + 1);
    if (!name_copy)
        return;
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
            char* path_copy = strdup(parser->source_path);
            if (path_copy) {
                char* dir = dirname(path_copy);
                snprintf(path, path_size, "%s/%.*s", dir, (int)module_length, module_name);
                free(path_copy);
                return;
            }
        }
        // Fallback if no source path: use path as-is relative to cwd
        snprintf(path, path_size, "%.*s", (int)module_length, module_name);
        return;
    }

    // Standard library import: try lib/ directories
    if (parser->source_path) {
        // Make a copy because dirname may modify its argument
        char* path_copy = strdup(parser->source_path);
        if (path_copy) {
            char* dir = dirname(path_copy);
            snprintf(path, path_size, "%s/lib/%.*s.w", dir, (int)module_length, module_name);
            free(path_copy);
            if (file_exists(path)) {
                return;
            }
            // Fall through to try cwd-relative path
        }
    }
    // Fallback: use lib/ relative to current working directory
    snprintf(path, path_size, "lib/%.*s.w", (int)module_length, module_name);
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
    parser_init_with_path(&import_parser, source, path);

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
                        imported_module->as.module.name = malloc(module_length + 1);
                        if (imported_module->as.module.name) {
                            memcpy(imported_module->as.module.name, module_name, module_length);
                            imported_module->as.module.name[module_length] = '\0';
                            imported_module->as.module.name_length         = (int)module_length;
                        }
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
    parser_init_with_path(parser, source, NULL);
}

void parser_init_with_path(Parser* parser, const char* source, const char* source_path) {
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parse_depth          = 0; // Reset recursion depth

    parser->source_path = source_path;

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
    if (!program) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&program->as.program.modules);

    // Create main module for the entry file
    Node* main_module = node_new(NODE_MODULE, 1, 1);
    if (!main_module) {
        parse_error(parser, "Out of memory");
        node_free(program);
        return NULL;
    }
    main_module->as.module.name        = strdup("main");
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

        Node* decl = parse_declaration(parser);
        if (decl) {
            nodelist_push(&main_module->as.module.decls, decl);
        }
        if (parser->panic_mode)
            synchronize(parser);
    }

    return program;
}
