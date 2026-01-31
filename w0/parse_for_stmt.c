#include "ast.h"
#include "parse_block.h"
#include "parse_expression.h"
#include "parse_var_decl.h"
#include "parser.h"
#include "parser_util.h"

Node* parse_for_stmt(Parser* parser) {
    Token token = parser->previous;
    consume(parser, TOK_LPAREN, "Expected '(' after 'for'");

    // Init
    Node* init = NULL;
    if (match(parser, TOK_VAR)) {
        init = parse_var_decl(parser, 0, 0);
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
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.for_stmt.init = init;
    node->as.for_stmt.cond = cond;
    node->as.for_stmt.post = post;
    node->as.for_stmt.body = body;
    return node;
}
