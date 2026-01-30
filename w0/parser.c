#include "parser.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Forward declarations
static Node* parse_declaration(Parser* parser);
static Node* parse_statement(Parser* parser);
static Node* parse_expression(Parser* parser);
static Node* parse_type(Parser* parser);
static Node* parse_foreach_stmt(Parser* parser);
static Node* parse_struct_init(Parser* parser);

static void advance(Parser* parser) {
    parser->previous = parser->current;
    parser->current  = lexer_next(&parser->lexer);
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

    snprintf(parser->error_msg, sizeof(parser->error_msg), "[line %d:%d] Error", token->line,
             token->column);

    if (token->type == TOK_EOF) {
        snprintf(parser->error_msg + strlen(parser->error_msg),
                 sizeof(parser->error_msg) - strlen(parser->error_msg), " at end");
    } else if (token->type != TOK_ERROR) {
        snprintf(parser->error_msg + strlen(parser->error_msg),
                 sizeof(parser->error_msg) - strlen(parser->error_msg), " at '%.*s'",
                 (int)token->length, token->start);
    }

    snprintf(parser->error_msg + strlen(parser->error_msg),
             sizeof(parser->error_msg) - strlen(parser->error_msg), ": %s", message);

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
    memcpy(str, token->start, token->length);
    str[token->length] = '\0';
    return str;
}

// Expression parsing with precedence climbing

static Node* parse_primary(Parser* parser) {
    Token token = parser->current;

    if (match(parser, TOK_INT)) {
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
        node->as.int_lit.value = strtol(start, NULL, base);
        return node;
    }

    if (match(parser, TOK_FLOAT)) {
        Node* node               = node_new(NODE_FLOAT_LIT, token.line, token.column);
        node->as.float_lit.value = strtod(token.start, NULL);
        return node;
    }

    if (match(parser, TOK_STRING)) {
        Node* node = node_new(NODE_STRING_LIT, token.line, token.column);
        // Skip quotes
        node->as.string_lit.value = malloc(token.length - 1);
        memcpy(node->as.string_lit.value, token.start + 1, token.length - 2);
        node->as.string_lit.value[token.length - 2] = '\0';
        node->as.string_lit.length                  = token.length - 2;
        return node;
    }

    if (match(parser, TOK_CHAR)) {
        Node* node = node_new(NODE_CHAR_LIT, token.line, token.column);
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
        Node* node              = node_new(NODE_BOOL_LIT, token.line, token.column);
        node->as.bool_lit.value = (token.type == TOK_TRUE);
        return node;
    }

    if (match(parser, TOK_NULL)) {
        return node_new(NODE_NULL_LIT, token.line, token.column);
    }

    if (match(parser, TOK_IDENT)) {
        Node* node            = node_new(NODE_IDENT, token.line, token.column);
        node->as.ident.name   = copy_token_string(&token);
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

static Node* parse_foreach_stmt(Parser* parser);
static Node* parse_struct_init(Parser* parser) {
    Token start = parser->previous;
    Node* node  = node_new(NODE_STRUCT_INIT, start.line, start.column);
    nodelist_init(&node->as.struct_init.fields);

    if (!check(parser, TOK_RBRACE)) {
        for (;;) {
            Token field_name = parser->current;
            consume(parser, TOK_IDENT, "Expected field name in struct initializer");

            Node* field = node_new(NODE_FIELD_INIT, field_name.line, field_name.column);
            field->as.field_init.name        = copy_token_string(&field_name);
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
            Node* call         = node_new(NODE_CALL, expr->line, expr->column);
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
            Node* index            = node_new(NODE_INDEX, expr->line, expr->column);
            index->as.index.object = expr;
            index->as.index.index  = parse_expression(parser);
            consume(parser, TOK_RBRACKET, "Expected ']' after index");
            expr = index;
        } else if (match(parser, TOK_DOT)) {
            // Member access
            Token name = parser->current;
            consume(parser, TOK_IDENT, "Expected member name after '.'");
            Node* member             = node_new(NODE_MEMBER, expr->line, expr->column);
            member->as.member.object = expr;
            member->as.member.name   = copy_token_string(&name);
            member->as.member.length = name.length;
            member->as.member.arrow  = 0;
            expr                     = member;
        } else if (match(parser, TOK_ARROW)) {
            // Arrow member access
            Token name = parser->current;
            consume(parser, TOK_IDENT, "Expected member name after '->'");
            Node* member             = node_new(NODE_MEMBER, expr->line, expr->column);
            member->as.member.object = expr;
            member->as.member.name   = copy_token_string(&name);
            member->as.member.length = name.length;
            member->as.member.arrow  = 1;
            expr                     = member;
        } else if (match(parser, TOK_PLUS_PLUS) || match(parser, TOK_MINUS_MINUS)) {
            // Postfix increment/decrement
            TokenType op            = parser->previous.type;
            Node*     unary         = node_new(NODE_UNARY, expr->line, expr->column);
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
        Token op               = parser->previous;
        Node* operand          = parse_unary(parser);
        Node* node             = node_new(NODE_UNARY, op.line, op.column);
        node->as.unary.op      = op.type;
        node->as.unary.operand = operand;
        node->as.unary.postfix = 0;
        return node;
    }
    return parse_postfix(parser);
}

static Node* parse_binary(Parser* parser, int min_prec);

static int get_precedence(TokenType type) {
    switch (type) {
    case TOK_STAR:
    case TOK_SLASH:
    case TOK_PERCENT:
        return 12;
    case TOK_PLUS:
    case TOK_MINUS:
        return 11;
    case TOK_LT_LT:
    case TOK_GT_GT:
        return 10;
    case TOK_LT:
    case TOK_GT:
    case TOK_LT_EQ:
    case TOK_GT_EQ:
        return 9;
    case TOK_EQ_EQ:
    case TOK_BANG_EQ:
        return 8;
    case TOK_AMP:
        return 7;
    case TOK_CARET:
        return 6;
    case TOK_PIPE:
        return 5;
    case TOK_AMP_AMP:
        return 4;
    case TOK_PIPE_PIPE:
        return 3;
    default:
        return 0;
    }
}

static int is_binary_op(TokenType type) {
    return get_precedence(type) > 0;
}

static Node* parse_binary(Parser* parser, int min_prec) {
    Node* left = parse_unary(parser);
    if (!left)
        return NULL;

    while (is_binary_op(parser->current.type) && get_precedence(parser->current.type) >= min_prec) {
        Token op = parser->current;
        advance(parser);
        int   prec  = get_precedence(op.type);
        Node* right = parse_binary(parser, prec + 1);

        Node* binary            = node_new(NODE_BINARY, op.line, op.column);
        binary->as.binary.op    = op.type;
        binary->as.binary.left  = left;
        binary->as.binary.right = right;
        left                    = binary;
    }

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
        return 1;
    default:
        return 0;
    }
}

static Node* parse_assignment(Parser* parser) {
    Node* expr = parse_binary(parser, 1);
    if (!expr)
        return NULL;

    if (is_assign_op(parser->current.type)) {
        Token op = parser->current;
        advance(parser);
        Node* value = parse_assignment(parser); // Right associative

        Node* assign             = node_new(NODE_ASSIGN, op.line, op.column);
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
        Node* inner            = parse_type(parser);
        Node* node             = node_new(NODE_UNARY, token.line, token.column);
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

        Node* node            = node_new(NODE_INDEX, token.line, token.column);
        node->as.index.object = elem;
        node->as.index.index  = size;
        return node;
    }

    // Named type
    if (match(parser, TOK_IDENT)) {
        Node* node            = node_new(NODE_IDENT, token.line, token.column);
        node->as.ident.name   = copy_token_string(&token);
        node->as.ident.length = token.length;
        return node;
    }

    error(parser, "Expected type");
    return NULL;
}

// Statement parsing
static Node* parse_block(Parser* parser) {
    Node* block = node_new(NODE_BLOCK, parser->previous.line, parser->previous.column);
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

    Node* node                    = node_new(NODE_VAR_DECL, name.line, name.column);
    node->as.var_decl.name        = copy_token_string(&name);
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

    Node* node                  = node_new(NODE_IF, token.line, token.column);
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

    Node* node               = node_new(NODE_WHILE, token.line, token.column);
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

    Node* node             = node_new(NODE_FOR, token.line, token.column);
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

    Node* node                 = node_new(NODE_RETURN, token.line, token.column);
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
        return node_new(NODE_BREAK, token.line, token.column);
    }
    if (match(parser, TOK_CONTINUE)) {
        Token token = parser->previous;
        consume(parser, TOK_SEMICOLON, "Expected ';' after 'continue'");
        return node_new(NODE_CONTINUE, token.line, token.column);
    }
    if (match(parser, TOK_LBRACE)) {
        return parse_block(parser);
    }

    // Expression statement
    Node* expr = parse_expression(parser);
    consume(parser, TOK_SEMICOLON, "Expected ';' after expression");

    Node* node = node_new(NODE_EXPR_STMT, expr ? expr->line : 0, expr ? expr->column : 0);
    node->as.expr_stmt.expr = expr;
    return node;
}

// Declaration parsing
static Node* parse_func_decl(Parser* parser) {
    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected function name");

    Node* node                     = node_new(NODE_FUNC_DECL, name.line, name.column);
    node->as.func_decl.name        = copy_token_string(&name);
    node->as.func_decl.name_length = name.length;
    nodelist_init(&node->as.func_decl.params);

    consume(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check(parser, TOK_RPAREN)) {
        do {
            Token param_name = parser->current;
            consume(parser, TOK_IDENT, "Expected parameter name");

            Node* param                 = node_new(NODE_PARAM, param_name.line, param_name.column);
            param->as.param.name        = copy_token_string(&param_name);
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

    Node* node                       = node_new(NODE_STRUCT_DECL, name.line, name.column);
    node->as.struct_decl.name        = copy_token_string(&name);
    node->as.struct_decl.name_length = name.length;
    nodelist_init(&node->as.struct_decl.fields);

    consume(parser, TOK_LBRACE, "Expected '{' after struct name");

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        Token field_name = parser->current;
        consume(parser, TOK_IDENT, "Expected field name");

        Node* field                 = node_new(NODE_FIELD, field_name.line, field_name.column);
        field->as.field.name        = copy_token_string(&field_name);
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

    Node* node                     = node_new(NODE_ENUM_DECL, name.line, name.column);
    node->as.enum_decl.name        = copy_token_string(&name);
    node->as.enum_decl.name_length = name.length;
    nodelist_init(&node->as.enum_decl.values);

    consume(parser, TOK_LBRACE, "Expected '{' after enum name");

    while (!check(parser, TOK_RBRACE) && !check(parser, TOK_EOF)) {
        Token value_name = parser->current;
        consume(parser, TOK_IDENT, "Expected enum value name");

        Node* value            = node_new(NODE_IDENT, value_name.line, value_name.column);
        value->as.ident.name   = copy_token_string(&value_name);
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
    advance(parser); // Prime the parser
}

static Node* parse_foreach_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'foreach'");

    // Parse: const identifier (foreach variables are immutable)
    consume(parser, TOK_CONST, "Expected 'const' in foreach loop");
    consume(parser, TOK_IDENT, "Expected identifier after 'var'");

    Token var_token = parser->previous;
    char* var_name  = malloc(var_token.length + 1);
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

    Node* node                            = node_new(NODE_FOREACH, token.line, token.column);
    node->as.foreach_stmt.var_name        = var_name;
    node->as.foreach_stmt.var_name_length = var_token.length;
    node->as.foreach_stmt.start           = start;
    node->as.foreach_stmt.end             = end;
    node->as.foreach_stmt.body            = body;
    return node;
}

Node* parser_parse(Parser* parser) {
    Node* program = node_new(NODE_PROGRAM, 1, 1);
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
