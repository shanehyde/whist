#ifndef WHIST_CODEGEN_TYPES_H
#define WHIST_CODEGEN_TYPES_H

#include "codegen.h"

// Type query functions (defined in codegen_types.c)
int         is_enum_type_name(CodeGen* gen, const char* name);
int         enum_index(CodeGen* gen, const char* name);
int         enum_has_rc_fields(CodeGen* gen, const char* name);
int         enum_has_eq(CodeGen* gen, const char* name);
Node*       resolve_alias(CodeGen* gen, Node* type_node);
int         is_struct_type(CodeGen* gen, Node* type_node);
int         type_node_has_rc(CodeGen* gen, Node* type_node);
const char* resolve_enum_name(CodeGen* gen, Node* type_node);
int         codegen_is_type_variable(const char* name);

// Generic binding (defined in codegen_emit.c)
int codegen_extract_method_bindings(NodeList* pattern_args, Type** concrete_args, int arg_count,
                                    char*** out_params, Type*** out_args, int* out_count);

// Shared helpers (defined in codegen.c)
char* build_mangled_name_from_generic_node(CodeGen* gen, Node* type_node);

#endif
