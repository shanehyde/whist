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
    func_decl_node* fdn = &node->as.func_decl;

    fdn->is_public         = is_public;
    fdn->is_extern         = 0;
    fdn->receiver_type     = receiver_type;
    fdn->receiver_type_len = receiver_type_len;
    fdn->receiver_is_const = receiver_is_const;
    fdn->name              = copy_token_string(&name);
    if (!fdn->name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    fdn->name_length = name.length;
    nodelist_init(&fdn->params);

    consume(parser, TOK_LPAREN, "Expected '(' after function name");

    // Parameters
    if (!check(parser, TOK_RPAREN)) {
        do {
            // Check for 'const' modifier
            int param_is_const = 0;
            if (match(parser, TOK_CONST)) {
                param_is_const = 1;
            }

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
            param->as.param.is_const    = param_is_const;

            if (match(parser, TOK_COLON)) {
                param->as.param.type = parse_type(parser);
            }

            nodelist_push(&fdn->params, param);
        } while (match(parser, TOK_COMMA));
    }

    consume(parser, TOK_RPAREN, "Expected ')' after parameters");

    // Return type
    fdn->return_type = NULL;
    if (match(parser, TOK_COLON)) {
        fdn->return_type = parse_type(parser);
    }

    // Body
    if (check(parser, TOK_LBRACE) == 0) {
        consume(parser, TOK_SEMICOLON, "Expected ';' after function declaration");
        // extern function declaration
        fdn->body = NULL;
        return node;
    }

    consume(parser, TOK_LBRACE, "Expected '{' before function body");
    fdn->body = parse_block(parser);

    return node;
}
