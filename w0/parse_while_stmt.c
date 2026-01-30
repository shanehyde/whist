#include "parse_enum_decl.h"
#include "parse_expression.h"
#include "parser_util.h"

Node* parse_block(Parser* parser);

Node* parse_while_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'while'");
    Node* cond = parse_expression(parser);
    consume(parser, TOK_RPAREN, "Expected ')' after condition");

    consume(parser, TOK_LBRACE, "Expected '{' after while condition");
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
