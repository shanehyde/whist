#ifndef WHIST_PARSE_VAR_DECL_H
#define WHIST_PARSE_VAR_DECL_H

#include "ast.h"
#include "parser.h"

Node* parse_var_decl(Parser* parser, int is_const);

#endif
