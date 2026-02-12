#ifndef WHIST_CHECKER_INTERNAL_H
#define WHIST_CHECKER_INTERNAL_H

#include "checker.h"

// --- From checker.c: error reporting ---
void check_error(Checker* checker, int line, int col, const char* fmt, ...);
void check_error_type(Checker* checker, int line, int col, const char* context, Type* expected,
                      Type* got);
void check_error_cannot(Checker* checker, int line, int col, const char* action, Type* type);

// --- From checker.c: statement checking ---
void check_statement(Checker* checker, Node* node);

// --- From checker_types.c: type resolution ---
Type* resolve_type(Checker* checker, Node* type_node);
Type* instantiate_generic_enum(Checker* checker, GenericDef* def, char* mangled,
                               Type** resolved_args, int arg_count);

// --- From checker_types.c: generic definition management ---
void             register_generic_def(Checker* checker, const char* name, char** type_params,
                                      char** type_param_bounds, int type_param_count, Node* decl);
void             register_generic_method(GenericDef* def, Node* method);
GenericDef*      lookup_generic_def(Checker* checker, const char* name);
GenericInstance* lookup_generic_instance(Checker* checker, const char* mangled_name);

// --- From checker_expr.c: expression checking ---
Type* check_expression(Checker* checker, Node* node);

// --- From checker.c: shared match-expression checking ---
Type* check_match_expr(Checker* checker, Node* node);

#endif
