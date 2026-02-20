#ifndef WHIST_PRINT_AST_H
#define WHIST_PRINT_AST_H

#include "ast.h"

void  print_ast(Node* node, int depth);
void  print_ast_checked(Node* node, int depth);
char* stringify_expr(Node* node);

#endif
