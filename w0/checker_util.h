#ifndef WHIST_CHECKER_UTIL_H
#define WHIST_CHECKER_UTIL_H

#include "ast.h"
#include "checker.h"

#define SCOPE_SIZE 64

void         check_error(Checker* checker, int line, int col, const char* fmt, ...);
unsigned int hash_string(const char* str);
void         checker_init(Checker* checker);
void         checker_free(Checker* checker);
void         checker_push_scope(Checker* checker);
void         checker_pop_scope(Checker* checker);
Type*        resolve_type(Checker* checker, Node* type_node);

#endif
