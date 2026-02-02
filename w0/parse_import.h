#ifndef WHIST_PARSE_IMPORT_H
#define WHIST_PARSE_IMPORT_H

#include "ast.h"
#include "parser.h"

int parse_import_stmt(Parser* parser, Node* program, Node* current_module);

#endif
