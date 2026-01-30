#include "parse_statement.h"

#include "parse_block.h"
#include "parse_expression.h"
#include "parse_for_stmt.h"
#include "parse_foreach_stmt.h"
#include "parse_if_stmt.h"
#include "parse_return_stmt.h"
#include "parse_var_decl.h"
#include "parse_while_stmt.h"
#include "parser_util.h"

Node* parse_statement(Parser* parser) {
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
            parse_error(parser, "Out of memory");
            return NULL;
        }
        return node;
    }
    if (match(parser, TOK_CONTINUE)) {
        Token token = parser->previous;
        consume(parser, TOK_SEMICOLON, "Expected ';' after 'continue'");
        Node* node = node_new(NODE_CONTINUE, token.line, token.column);
        if (!node) {
            parse_error(parser, "Out of memory");
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
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.expr_stmt.expr = expr;
    return node;
}
