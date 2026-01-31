#include <stdlib.h>

#include "parse_enum_decl.h"
#include "parser_util.h"

Node* parse_type(Parser* parser);
Node* parse_block(Parser* parser);

// Declaration parsing
Node* parse_func_decl(Parser* parser, int is_public) {
    // Check for method receiver: func (Type) or func (const Type)
    char* receiver_type     = NULL;
    int   receiver_type_len = 0;
    int   receiver_is_const = 0;

    if (check(parser, TOK_LPAREN)) {
        advance(parser); // consume '('

        // Check for 'const' modifier
        if (match(parser, TOK_CONST)) {
            receiver_is_const = 1;
        }

        // Expect struct type name
        Token recv_type = parser->current;
        consume(parser, TOK_IDENT, "Expected receiver type name");
        receiver_type = copy_token_string(&recv_type);
        if (!receiver_type) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        receiver_type_len = recv_type.length;

        consume(parser, TOK_RPAREN, "Expected ')' after receiver type");
    }

    Token name = parser->current;
    consume(parser, TOK_IDENT, "Expected function name");

    Node* node = node_new(NODE_FUNC_DECL, name.line, name.column);
    if (!node) {
        free(receiver_type);
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.func_decl.is_public         = is_public;
    node->as.func_decl.receiver_type     = receiver_type;
    node->as.func_decl.receiver_type_len = receiver_type_len;
    node->as.func_decl.receiver_is_const = receiver_is_const;
    node->as.func_decl.name              = copy_token_string(&name);
    if (!node->as.func_decl.name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    node->as.func_decl.name_length = name.length;
    nodelist_init(&node->as.func_decl.params);

    consume(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check(parser, TOK_RPAREN)) {
        do {
            Token param_name = parser->current;
            consume(parser, TOK_IDENT, "Expected parameter name");

            Node* param = node_new(NODE_PARAM, param_name.line, param_name.column);
            if (!param) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
            param->as.param.name = copy_token_string(&param_name);
            if (!param->as.param.name) {
                parse_error(parser, "Out of memory");
                return NULL;
            }
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
