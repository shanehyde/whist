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
Type*        resolve_type(Checker* checker, Node* type_node);
Type*        ensure_vec_type(Checker* checker, Type* elem_type);
Type*        ensure_box_type(Checker* checker, Type* elem_type);
Type*        instantiate_generic_enum(Checker* checker, GenericDef* def, char* mangled,
                                      Type** resolved_args, int arg_count);
VecInstance* lookup_vec_instance_pub(Checker* checker, const char* mangled_name);
void         ensure_vec_user_methods(Checker* checker, VecInstance* inst);
void         ensure_generic_enum_methods(Checker* checker, Type* enum_type);

// --- From checker_types.c: generic definition management ---
void             register_generic_def(Checker* checker, const char* name, char** type_params,
                                      char** type_param_bounds, int type_param_count, Node* decl);
void             register_generic_method(GenericDef* def, Node* method);
GenericDef*      lookup_generic_def(Checker* checker, const char* name);
GenericInstance* lookup_generic_instance(Checker* checker, const char* mangled_name);

// --- From checker_types.c: generic free function management ---
void register_generic_func_def(Checker* checker, const char* name, char** type_params,
                               char** type_param_bounds, int type_param_count, Node* decl);
GenericFuncDef*      lookup_generic_func_def(Checker* checker, const char* name);
GenericFuncInstance* lookup_generic_func_instance(Checker* checker, const char* mangled_name);
GenericFuncInstance* instantiate_generic_func(Checker* checker, GenericFuncDef* def,
                                              Type** type_args, int type_arg_count, int line,
                                              int col);

// --- From checker_types.c: method-level generic function management ---
void            register_generic_method_func_def(Checker* checker, const char* receiver_type,
                                                 const char* method_name, char** combined_params,
                                                 char** combined_bounds, int combined_count,
                                                 int receiver_param_count, Node* decl);
GenericFuncDef* lookup_generic_method_func_def(Checker* checker, const char* receiver_type,
                                               const char* method_name);
GenericFuncInstance* instantiate_generic_method_func(Checker* checker, GenericFuncDef* def,
                                                     Type** combined_args, int combined_count,
                                                     Type* receiver_concrete, int line, int col);

// --- From checker_expr.c: expression checking ---
Type* check_expression(Checker* checker, Node* node);

// --- From checker.c: shared match-expression checking ---
Type* check_match_expr(Checker* checker, Node* node);

#endif
