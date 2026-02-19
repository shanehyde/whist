#ifndef WHIST_PARSER_INTERNAL_H
#define WHIST_PARSER_INTERNAL_H

#include "parser.h"

Node* parse_type(Parser* parser);
Node* parse_expression(Parser* parser);
Node* parse_block(Parser* parser);
Node* parse_match(Parser* parser, int is_expr);
Node* parse_var_decl(Parser* parser, int is_const, int is_public);
Node* parse_use_stmt(Parser* parser);
int   parse_import_stmt(Parser* parser, Node* program, Node* current_module);
int   parse_include_stmt(Parser* parser, Node* program, Node* current_module);
Node* parse_declaration(Parser* parser);

#endif
