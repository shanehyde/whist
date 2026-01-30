#include "parse_block.h"
#include "parser_util.h"

Node* parse_statement(Parser* parser);

Node* parse_block(Parser* parser) {
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
