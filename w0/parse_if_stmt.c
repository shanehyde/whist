#include "parse_if_stmt.h"

#include "parse_block.h"
#include "parse_expression.h"
#include "parser_util.h"

Node* parse_if_stmt(Parser* parser) {
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
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.if_stmt.cond       = cond;
    node->as.if_stmt.then_block = then_block;
    node->as.if_stmt.else_block = else_block;
    return node;
}
