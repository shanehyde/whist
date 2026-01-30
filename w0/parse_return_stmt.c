#include "parse_return_stmt.h"

#include "parse_expression.h"
#include "parser_util.h"

Node* parse_return_stmt(Parser* parser) {
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
