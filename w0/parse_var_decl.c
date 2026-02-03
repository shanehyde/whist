#include "parse_var_decl.h"

#include <stdlib.h>
#include <string.h>

#include "parse_expression.h"
#include "parse_type.h"
#include "parser_util.h"

Node* parse_var_decl(Parser* parser, int is_const, int is_public) {
    Token start = parser->current;

    // Check for destructuring: var (a, b) = ...
    if (check_token(parser, TOK_LPAREN)) {
        advance_token(parser); // consume '('

        Node* node = node_new(NODE_VAR_DECL, start.line, start.column);
        if (!node) {
            parse_error(parser, "Out of memory");
            return NULL;
        }
        var_decl_node* vdn = &node->as.var_decl;

        vdn->name               = NULL; // NULL indicates destructuring
        vdn->name_length        = 0;
        vdn->is_public          = is_public;
        vdn->is_const           = is_const;
        vdn->type               = NULL;
        vdn->init               = NULL;
        vdn->destruct_names     = NULL;
        vdn->destruct_name_lens = NULL;
        vdn->destruct_count     = 0;

        // Collect names into a temporary list
        int    capacity = 4;
        char** names    = malloc(capacity * sizeof(char*));
        int*   lens     = malloc(capacity * sizeof(int));
        int    count    = 0;

        // Parse first name
        Token name_tok = parser->current;
        consume_token(parser, TOK_IDENT, "Expected variable name in destructuring pattern");

        names[count] = copy_token_string(&name_tok);
        lens[count]  = name_tok.length;
        count++;

        // Parse remaining names (comma-separated)
        while (match_token(parser, TOK_COMMA)) {
            if (count >= capacity) {
                capacity *= 2;
                names = realloc(names, capacity * sizeof(char*));
                lens  = realloc(lens, capacity * sizeof(int));
            }
            name_tok = parser->current;
            consume_token(parser, TOK_IDENT, "Expected variable name after ',' in destructuring");
            names[count] = copy_token_string(&name_tok);
            lens[count]  = name_tok.length;
            count++;
        }

        consume_token(parser, TOK_RPAREN, "Expected ')' after destructuring pattern");

        // Require at least 2 names for destructuring
        if (count < 2) {
            parse_error(parser, "Destructuring requires at least 2 variables");
            for (int i = 0; i < count; i++)
                free(names[i]);
            free(names);
            free(lens);
            node_free(node);
            return NULL;
        }

        vdn->destruct_names      = names;
        vdn->destruct_name_lens  = lens;
        vdn->destruct_count      = count;
        vdn->destruct_tuple_type = NULL; // Set by checker

        // Optional type annotation
        if (match_token(parser, TOK_COLON)) {
            vdn->type = parse_type(parser);
        }

        // Initializer is required for destructuring
        if (!match_token(parser, TOK_EQ)) {
            parse_error(parser, "Destructuring declaration requires an initializer");
            node_free(node);
            return NULL;
        }
        vdn->init = parse_expression(parser);

        consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
        return node;
    }

    // Normal variable declaration
    Token name = parser->current;
    consume_token(parser, TOK_IDENT, "Expected variable name");

    Node* node = node_new(NODE_VAR_DECL, name.line, name.column);
    if (!node) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    var_decl_node* vdn = &node->as.var_decl;

    vdn->name = copy_token_string(&name);
    if (!vdn->name) {
        parse_error(parser, "Out of memory");
        return NULL;
    }
    vdn->is_public           = is_public;
    vdn->name_length         = name.length;
    vdn->is_const            = is_const;
    vdn->type                = NULL;
    vdn->init                = NULL;
    vdn->destruct_names      = NULL;
    vdn->destruct_name_lens  = NULL;
    vdn->destruct_count      = 0;
    vdn->destruct_tuple_type = NULL;

    // Optional type annotation
    if (match_token(parser, TOK_COLON)) {
        vdn->type = parse_type(parser);
    }

    // Optional initializer
    if (match_token(parser, TOK_EQ)) {
        vdn->init = parse_expression(parser);
    }

    consume_token(parser, TOK_SEMICOLON, "Expected ';' after variable declaration");
    return node;
}
