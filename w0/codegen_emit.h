#ifndef WHIST_CODEGEN_EMIT_H
#define WHIST_CODEGEN_EMIT_H

#include "codegen.h"

// Output helpers
void emit_indent(CodeGen* gen);
void emit(CodeGen* gen, const char* fmt, ...);

// Type emission
void emit_type(CodeGen* gen, Node* type_node);
void emit_resolved_type(CodeGen* gen, Type* type);
void emit_type_with_name(CodeGen* gen, Node* type_node, const char* name);
void emit_function_name(CodeGen* gen, const char* func_name, const char* receiver_type,
                        const char* module_name);

// Expression, statement, declaration emission
void emit_stmt(CodeGen* gen, Node* node);
void emit_decl(CodeGen* gen, Node* node);

// RC helpers
void rc_clear_all(CodeGen* gen);

// Defer helpers
void defer_clear(CodeGen* gen);

// Typedef emission (defined in codegen_emit.c)
void emit_bounds_checks(CodeGen* gen);
void emit_string_helpers(CodeGen* gen);
void emit_tuple_typedefs(CodeGen* gen);
void emit_struct_forward_decls(CodeGen* gen, Node* ast);
void emit_span_typedefs(CodeGen* gen);
void emit_vec_typedefs(CodeGen* gen);
void emit_enum_typedefs(CodeGen* gen, Node* ast);
void emit_generic_enum_typedefs(CodeGen* gen, Node* ast);
void emit_struct_body_typedefs(CodeGen* gen, Node* ast);
void emit_function_forward_decls(CodeGen* gen, Node* ast);
void emit_enum_eq_helpers(CodeGen* gen, Node* ast);

// RC emission (defined in codegen_rc.c)
void emit_rc_runtime(CodeGen* gen);
void emit_enum_rc_helpers(CodeGen* gen, Node* ast);
void emit_struct_cleanup(CodeGen* gen, Node* ast);
void emit_vec_methods(CodeGen* gen);
void emit_vec_cleanup(CodeGen* gen);

#endif
