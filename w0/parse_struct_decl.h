#ifndef WHIST_PARSE_STRUCT_DECL_H
#define WHIST_PARSE_STRUCT_DECL_H

#include "ast.h"
#include "parser.h"

Node* parse_struct_decl(Parser* parser, int is_public);

#endif
