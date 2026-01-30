#ifndef WHIST_CHECK_STRUCT_INIT_H
#define WHIST_CHECK_STRUCT_INIT_H

#include "ast.h"
#include "checker.h"

Type* check_struct_init(Checker* checker, Node* init, Type* struct_type);

#endif
