#include "parse_foreach_stmt.h"

#include <stdlib.h>
#include <string.h>

#include "parse_block.h"
#include "parse_expression.h"
#include "parser_util.h"

Node* parse_foreach_stmt(Parser* parser) {
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
