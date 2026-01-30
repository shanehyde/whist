#include "parse_expression.h"

#include "parse_primary.h"
#include "parser_util.h"

static Node* parse_postfix(Parser* parser) {
    Node* expr = parse_primary(parser);
    if (!expr)
        return NULL;

    for (;;) {
        if (match(parser, TOK_LPAREN)) {
            // Function call
            Node* call = node_new(NODE_CALL, expr->line, expr->column);
            if (!call) {
                parse_error(parser, "Out of memory");
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
                parse_error(parser, "Out of memory");
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
            member->as.member.arrow  = 0;
            expr                     = member;
        } else if (match(parser, TOK_ARROW)) {
            // Arrow member access
            Token name = parser->current;
            consume(parser, TOK_IDENT, "Expected member name after '->'");
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
            member->as.member.arrow  = 1;
            expr                     = member;
        } else if (match(parser, TOK_PLUS_PLUS) || match(parser, TOK_MINUS_MINUS)) {
            // Postfix increment/decrement
            TokenType op    = parser->previous.type;
            Node*     unary = node_new(NODE_UNARY, expr->line, expr->column);
            if (!unary) {
                parse_error(parser, "Out of memory");
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
            parse_error(parser, "Out of memory");
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
        advance(parser);
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

Node* parse_expression(Parser* parser) {
    return parse_assignment(parser);
}