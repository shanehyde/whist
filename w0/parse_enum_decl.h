#ifndef WHIST_PARSE_ENUM_DECL_H
#define WHIST_PARSE_ENUM_DECL_H

#include "ast.h"
#include "parser.h"

Node* parse_enum_decl(Parser* parser, int is_public);

#endif
