#include "parser.h"
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Maximum recursion depth to prevent stack overflow
#define MAX_PARSE_DEPTH 256

// Precedence levels for binary operators
typedef enum {
    PREC_NONE       = 0,
    PREC_OR         = 3,  // ||
    PREC_AND        = 4,  // &&
    PREC_BIT_OR     = 5,  // |
    PREC_BIT_XOR    = 6,  // ^
    PREC_BIT_AND    = 7,  // &
    PREC_EQUALITY   = 8,  // == !=
    PREC_COMPARISON = 9,  // < > <= >=
    PREC_SHIFT      = 10, // << >>
    PREC_TERM       = 11, // + -
    PREC_FACTOR     = 12, // * / %
} Precedence;

// Current recursion depth for expression parsing
static int parse_depth = 0;

// Forward declarations
static Node* parse_declaration(Parser* parser);
static Node* parse_statement(Parser* parser);
static Node* parse_expression(Parser* parser);
static Node* parse_type(Parser* parser);
static Node* parse_foreach_stmt(Parser* parser);
static Node* parse_struct_init(Parser* parser);

static void advance(Parser* parser) {
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

static int check(Parser* parser, TokenType type) {
    return parser->current.type == type;
}

static int match(Parser* parser, TokenType type) {
    if (!check(parser, type))
        return 0;
    advance(parser);
    return 1;
}

static void error_at(Parser* parser, Token* token, const char* message) {
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

static void error(Parser* parser, const char* message) {
    error_at(parser, &parser->current, message);
}

static void consume(Parser* parser, TokenType type, const char* message) {
    if (parser->current.type == type) {
        advance(parser);
        return;
    }
    error(parser, message);
}

static void synchronize(Parser* parser) {
    parser->panic_mode = 0;

    // Always advance at least once to ensure progress and prevent infinite loops
    if (parser->current.type != TOK_EOF) {
        advance(parser);
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
        advance(parser);
    }
}

static char* copy_token_string(Token* token) {
    char* str = malloc(token->length + 1);
    if (!str) {
        return NULL;
    }
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

// Expression parsing with precedence climbing

static Node* parse_primary(Parser* parser) {
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

static Node* parse_struct_init(Parser* parser) {
    Token start = parser->previous;
    Node* node  = node_new(NODE_STRUCT_INIT, start.line, start.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&node->as.struct_init.fields);

    if (!check(parser, TOK_RBRACE)) {
        for (;;) {
            Token field_name = parser->current;
            consume(parser, TOK_IDENT, "Expected field name in struct initializer");

            Node* field = node_new(NODE_FIELD_INIT, field_name.line, field_name.column);
            if (!field) {
                error(parser, "Out of memory");
                return NULL;
            }
            field->as.field_init.name = copy_token_string(&field_name);
            if (!field->as.field_init.name) {
                error(parser, "Out of memory");
                return NULL;
            }
            field->as.field_init.name_length = field_name.length;

            consume(parser, TOK_COLON, "Expected ':' after field name");
            field->as.field_init.value = parse_expression(parser);

            nodelist_push(&node->as.struct_init.fields, field);

            if (match(parser, TOK_COMMA)) {
                if (check(parser, TOK_RBRACE)) {
                    break; // trailing comma
                }
                continue;
            }
            break;
        }
    }

    consume(parser, TOK_RBRACE, "Expected '}' after struct initializer");
    return node;
}

static Node* parse_postfix(Parser* parser) {
    Node* expr = parse_primary(parser);
    if (!expr)
        return NULL;

    for (;;) {
        if (match(parser, TOK_LPAREN)) {
            // Function call
            Node* call = node_new(NODE_CALL, expr->line, expr->column);
            if (!call) {
                error(parser, "Out of memory");
                return NULL;
            }
            call->as.call.func = expr;
            nodelist_init(&call->as.call.args);

            if (!check(parser, TOK_RPAREN)) {
                do {
                    Node* arg = parse_expression(parser);
                    if (arg)
                        nodelist_push(&call->as.call.args, arg);
                } while (match(parser, TOK_COMMA));
            }
            consume(parser, TOK_RPAREN, "Expected ')' after arguments");
            expr = call;
        } else if (match(parser, TOK_LBRACKET)) {
            // Index
            Node* index = node_new(NODE_INDEX, expr->line, expr->column);
            if (!index) {
                error(parser, "Out of memory");
                return NULL;
            }
            index->as.index.object = expr;
            index->as.index.index  = parse_expression(parser);
            consume(parser, TOK_RBRACKET, "Expected ']' after index");
            expr = index;
        } else if (match(parser, TOK_DOT)) {
            // Member access
            Token name = parser->current;
            consume(parser, TOK_IDENT, "Expected member name after '.'");
            Node* member = node_new(NODE_MEMBER, expr->line, expr->column);
            if (!member) {
                error(parser, "Out of memory");
                return NULL;
            }
            member->as.member.object = expr;
            member->as.member.name   = copy_token_string(&name);
            if (!member->as.member.name) {
                error(parser, "Out of memory");
                return NULL;
            }
            member->as.member.length = name.length;
            member->as.member.arrow  = 0;
            expr                     = member;
        } else if (match(parser, TOK_ARROW)) {
            // Arrow member access
            Token name = parser->current;
            consume(parser, TOK_IDENT, "Expected member name after '->'");
            Node* member = node_new(NODE_MEMBER, expr->line, expr->column);
            if (!member) {
                error(parser, "Out of memory");
                return NULL;
            }
            member->as.member.object = expr;
            member->as.member.name   = copy_token_string(&name);
            if (!member->as.member.name) {
                error(parser, "Out of memory");
                return NULL;
            }
            member->as.member.length = name.length;
            member->as.member.arrow  = 1;
            expr                     = member;
        } else if (match(parser, TOK_PLUS_PLUS) || match(parser, TOK_MINUS_MINUS)) {
            // Postfix increment/decrement
            TokenType op    = parser->previous.type;
            Node*     unary = node_new(NODE_UNARY, expr->line, expr->column);
            if (!unary) {
                error(parser, "Out of memory");
                return NULL;
            }
            unary->as.unary.op      = op;
            unary->as.unary.operand = expr;
            unary->as.unary.postfix = 1;
            expr                    = unary;
        } else {
            break;
        }
    }

    return expr;
}

static Node* parse_unary(Parser* parser) {
    if (match(parser, TOK_BANG) || match(parser, TOK_MINUS) || match(parser, TOK_TILDE) ||
        match(parser, TOK_AMP) || match(parser, TOK_STAR) || match(parser, TOK_PLUS_PLUS) ||
        match(parser, TOK_MINUS_MINUS)) {
        Token op      = parser->previous;
        Node* operand = parse_unary(parser);
        Node* node    = node_new(NODE_UNARY, op.line, op.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.unary.op      = op.type;
        node->as.unary.operand = operand;
        node->as.unary.postfix = 0;
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
        error(parser, "Maximum expression nesting depth exceeded");
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
        advance(parser);
        Precedence prec  = get_precedence(op.type);
        Node*      right = parse_binary(parser, prec + 1);

        Node* binary = node_new(NODE_BINARY, op.line, op.column);
        if (!binary) {
            error(parser, "Out of memory");
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

static Node* parse_assignment(Parser* parser) {
    Node* expr = parse_binary(parser, PREC_OR);
    if (!expr)
        return NULL;

    if (is_assign_op(parser->current.type)) {
        Token op = parser->current;
        advance(parser);
        Node* value = parse_assignment(parser); // Right associative

        Node* assign = node_new(NODE_ASSIGN, op.line, op.column);
        if (!assign) {
            error(parser, "Out of memory");
            return NULL;
        }
        assign->as.assign.op     = op.type;
        assign->as.assign.target = expr;
        assign->as.assign.value  = value;
        return assign;
    }

    return expr;
}

static Node* parse_expression(Parser* parser) {
    return parse_assignment(parser);
}

// Type parsing
static Node* parse_type(Parser* parser) {
    Token token = parser->current;

    // Pointer type
    if (match(parser, TOK_STAR)) {
        Node* inner = parse_type(parser);
        Node* node  = node_new(NODE_UNARY, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        node->as.unary.op      = TOK_STAR;
        node->as.unary.operand = inner;
        return node;
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
            error(parser, "Out of memory");
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

    error(parser, "Expected type");
    return NULL;
}

// Statement parsing
static Node* parse_block(Parser* parser) {
    Node* block = node_new(NODE_BLOCK, parser->previous.line, parser->previous.column);
    if (!block) {
        error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&block->as.block.stmts);

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        Node* stmt = parse_statement(parser);
        if (stmt)
            nodelist_push(&block->as.block.stmts, stmt);
        if (parser->panic_mode)
            synchronize(parser);
    }

    consume(parser, TOK_RBRACE, "Expected '}' after block");
    return block;
}

static Node* parse_var_decl(Parser* parser, int is_const) {
    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected variable name");

    Node* node = node_new(NODE_VAR_DECL, name.line, name.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.var_decl.name = copy_token_string(&name);
    if (!node->as.var_decl.name) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.var_decl.name_length = name.length;
    node->as.var_decl.is_const    = is_const;
    node->as.var_decl.type        = NULL;
    node->as.var_decl.init        = NULL;

    // Optional type annotation
    if (match(parser, TOK_COLON)) {
        node->as.var_decl.type = parse_type(parser);
    }

    // Optional initializer
    if (match(parser, TOK_EQ)) {
        node->as.var_decl.init = parse_expression(parser);
    }

    consume(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}

static Node* parse_if_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'if'");
    Node* cond = parse_expression(parser);
    consume(parser, TOK_RPAREN, "Expected ')' after condition");

    consume(parser, TOK_LBRACE, "Expected '{' after if condition");
    Node* then_block = parse_block(parser);

    Node* else_block = NULL;
    if (match(parser, TOK_ELSE)) {
        if (match(parser, TOK_IF)) {
            else_block = parse_if_stmt(parser);
        } else {
            consume(parser, TOK_LBRACE, "Expected '{' after 'else'");
            else_block = parse_block(parser);
        }
    }

    Node* node = node_new(NODE_IF, token.line, token.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.if_stmt.cond       = cond;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}

static Node* parse_while_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'while'");
    Node* cond = parse_expression(parser);
    consume(parser, TOK_RPAREN, "Expected ')' after condition");

    consume(parser, TOK_LBRACE, "Expected '{' after while condition");
    Node* body = parse_block(parser);

    Node* node = node_new(NODE_WHILE, token.line, token.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.while_stmt.cond = cond;
    node->as.while_stmt.body = body;
    return node;
}

static Node* parse_for_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'for'");

    // Init
    Node* init = NULL;
    if (match(parser, TOK_VAR)) {
        init = parse_var_decl(parser, 0);
    } else if (!match(parser, TOK_SEMICOLON)) {
        init = parse_expression(parser);
        consume(parser, TOK_SEMICOLON, "Expected ';' after for initializer");
    }

    // Condition
    Node* cond = NULL;
    if (!check(parser, TOK_SEMICOLON)) {
        cond = parse_expression(parser);
    }
    consume(parser, TOK_SEMICOLON, "Expected ';' after for condition");

    // Post
    Node* post = NULL;
    if (!check(parser, TOK_RPAREN)) {
        post = parse_expression(parser);
    }
    consume(parser, TOK_RPAREN, "Expected ')' after for clauses");

    consume(parser, TOK_LBRACE, "Expected '{' after for clauses");
    Node* body = parse_block(parser);

    Node* node = node_new(NODE_FOR, token.line, token.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.for_stmt.init = init;
    node->as.for_stmt.cond = cond;
    node->as.for_stmt.post = post;
    node->as.for_stmt.body = body;
    return node;
}

static Node* parse_return_stmt(Parser* parser) {
    Token token = parser->previous;
    Node* value = NULL;

    if (!check(parser, TOK_SEMICOLON)) {
        value = parse_expression(parser);
    }
    consume(parser, TOK_SEMICOLON, "Expected ';' after return value");

    Node* node = node_new(NODE_RETURN, token.line, token.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.return_stmt.value = value;
    return node;
}

static Node* parse_statement(Parser* parser) {
    if (match(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0);
    }
    if (match(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1);
    }
    if (match(parser, TOK_IF)) {
        return parse_if_stmt(parser);
    }
    if (match(parser, TOK_WHILE)) {
        return parse_while_stmt(parser);
    }
    if (match(parser, TOK_FOR)) {
        return parse_for_stmt(parser);
    }
    if (match(parser, TOK_FOREACH)) {
        return parse_foreach_stmt(parser);
    }
    if (match(parser, TOK_RETURN)) {
        return parse_return_stmt(parser);
    }
    if (match(parser, TOK_BREAK)) {
        Token token = parser->previous;
        consume(parser, TOK_SEMICOLON, "Expected ';' after 'break'");
        Node* node = node_new(NODE_BREAK, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        return node;
    }
    if (match(parser, TOK_CONTINUE)) {
        Token token = parser->previous;
        consume(parser, TOK_SEMICOLON, "Expected ';' after 'continue'");
        Node* node = node_new(NODE_CONTINUE, token.line, token.column);
        if (!node) {
            error(parser, "Out of memory");
            return NULL;
        }
        return node;
    }
    if (match(parser, TOK_LBRACE)) {
        return parse_block(parser);
    }

    // Expression statement
    Node* expr = parse_expression(parser);
    consume(parser, TOK_SEMICOLON, "Expected ';' after expression");

    Node* node = node_new(NODE_EXPR_STMT, expr ? expr->line : 0, expr ? expr->column : 0);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.expr_stmt.expr = expr;
    return node;
}

// Declaration parsing
static Node* parse_func_decl(Parser* parser) {
    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected function name");

    Node* node = node_new(NODE_FUNC_DECL, name.line, name.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.func_decl.name = copy_token_string(&name);
    if (!node->as.func_decl.name) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.func_decl.name_length = name.length;
    nodelist_init(&node->as.func_decl.params);

    consume(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check(parser, TOK_RPAREN)) {
        do {
            Token param_name = parser->current;
            consume(parser, TOK_IDENT, "Expected parameter name");

            Node* param = node_new(NODE_PARAM, param_name.line, param_name.column);
            if (!param) {
                error(parser, "Out of memory");
                return NULL;
            }
            param->as.param.name = copy_token_string(&param_name);
            if (!param->as.param.name) {
                error(parser, "Out of memory");
                return NULL;
            }
            param->as.param.name_length = param_name.length;
            param->as.param.type        = NULL;

            if (match(parser, TOK_COLON)) {
                param->as.param.type = parse_type(parser);
            }

            nodelist_push(&node->as.func_decl.params, param);
        } while (match(parser, TOK_COMMA));
    }

    consume(parser, TOK_RPAREN, "Expected ')' after parameters");

    // Return type
    node->as.func_decl.return_type = NULL;
    if (match(parser, TOK_COLON)) {
        node->as.func_decl.return_type = parse_type(parser);
    }

    // Body
    consume(parser, TOK_LBRACE, "Expected '{' before function body");
    node->as.func_decl.body = parse_block(parser);

    return node;
}

static Node* parse_struct_decl(Parser* parser) {
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

static Node* parse_enum_decl(Parser* parser) {
    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected enum name");

    Node* node = node_new(NODE_ENUM_DECL, name.line, name.column);
    if (!node) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.enum_decl.name = copy_token_string(&name);
    if (!node->as.enum_decl.name) {
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.enum_decl.name_length = name.length;
    nodelist_init(&node->as.enum_decl.values);

    consume(parser, TOK_LBRACE, "Expected '{' after enum name");

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        Token value_name = parser->current;
        consume(parser, TOK_IDENT, "Expected enum value name");

        Node* value = node_new(NODE_IDENT, value_name.line, value_name.column);
        if (!value) {
            error(parser, "Out of memory");
            return NULL;
        }
        value->as.ident.name = copy_token_string(&value_name);
        if (!value->as.ident.name) {
            error(parser, "Out of memory");
            return NULL;
        }
        value->as.ident.length = value_name.length;

        nodelist_push(&node->as.enum_decl.values, value);

        if (!check(parser, TOK_RBRACE)) {
            consume(parser, TOK_COMMA, "Expected ',' or '}' after enum value");
        } else {
            match(parser, TOK_COMMA); // Allow trailing comma
        }
    }

    consume(parser, TOK_RBRACE, "Expected '}' after enum values");
    return node;
}

static Node* parse_declaration(Parser* parser) {
    if (match(parser, TOK_FUNC)) {
        return parse_func_decl(parser);
    }
    if (match(parser, TOK_STRUCT)) {
        return parse_struct_decl(parser);
    }
    if (match(parser, TOK_ENUM)) {
        return parse_enum_decl(parser);
    }
    if (match(parser, TOK_VAR)) {
        return parse_var_decl(parser, 0);
    }
    if (match(parser, TOK_CONST)) {
        return parse_var_decl(parser, 1);
    }

    error(parser, "Expected declaration");
    return NULL;
}

void parser_init(Parser* parser, const char* source) {
    lexer_init(&parser->lexer, source);
    parser->had_error    = 0;
    parser->panic_mode   = 0;
    parser->error_msg[0] = '\0';
    parse_depth          = 0; // Reset recursion depth
    advance(parser);          // Prime the parser
}

static Node* parse_foreach_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'foreach'");

    // Parse: const identifier (foreach variables are immutable)
    consume(parser, TOK_CONST, "Expected 'const' in foreach loop");
    consume(parser, TOK_IDENT, "Expected identifier after 'const'");

    Token var_token = parser->previous;
    char* var_name  = malloc(var_token.length + 1);
    if (!var_name) {
        error(parser, "Out of memory");
        return NULL;
    }
    memcpy(var_name, var_token.start, var_token.length);
    var_name[var_token.length] = '\0';

    // Parse: in
    consume(parser, TOK_IN, "Expected 'in' after foreach variable");

    // Parse: start expression
    Node* start = parse_expression(parser);

    // Parse: ..
    consume(parser, TOK_DOT_DOT, "Expected '..' in range expression");

    // Parse: end expression
    Node* end = parse_expression(parser);

    consume(parser, TOK_RPAREN, "Expected ')' after foreach clauses");
    consume(parser, TOK_LBRACE, "Expected '{' after foreach clauses");
    Node* body = parse_block(parser);

    Node* node = node_new(NODE_FOREACH, token.line, token.column);
    if (!node) {
        free(var_name);
        error(parser, "Out of memory");
        return NULL;
    }
    node->as.foreach_stmt.var_name        = var_name;
    node->as.foreach_stmt.var_name_length = var_token.length;
    node->as.foreach_stmt.start           = start;
    node->as.foreach_stmt.end             = end;
    node->as.foreach_stmt.body            = body;
    return node;
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
    if (!program) {
        error(parser, "Out of memory");
        return NULL;
    }
    nodelist_init(&program->as.program.decls);

    while (!check(parser, TOK_EOF)) {
        Node* decl = parse_declaration(parser);
        if (decl) {
            nodelist_push(&program->as.program.decls, decl);
        }
        if (parser->panic_mode)
            synchronize(parser);
    }

    return program;
}
