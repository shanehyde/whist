#include "parse_defer_stmt.h"

#include "parse_statement.h"
#include "parser_util.h"

Node* parse_defer_stmt(Parser* parser) {
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
