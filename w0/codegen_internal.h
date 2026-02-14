#ifndef WHIST_CODEGEN_INTERNAL_H
#define WHIST_CODEGEN_INTERNAL_H

#include "codegen_emit.h"
#include "codegen_types.h"

// --- From codegen.c: generic substitution ---
Type* subst_lookup(CodeGen* gen, const char* name);
char* stringify_expr(Node* node);

// --- From codegen_emit.c: shared helpers ---
void        defer_push(CodeGen* gen, Node* node);
const char* binary_op_str(TokenType op);
const char* unary_op_str(TokenType op);
const char* assign_op_str(TokenType op);

// --- From codegen_rc.c: RC variable tracking ---
const char* get_dec_func_for_type(Type* t);
const char* get_inc_func_for_type(Type* t);
void        rc_push_var(CodeGen* gen, const char* name, const char* dec_func, Type* type);
void        rc_cleanup_scope(CodeGen* gen, int depth);
void        rc_cleanup_all(CodeGen* gen, const char* skip_name);
const char* rc_get_dec_func(CodeGen* gen, const char* name);
Type*       rc_get_var_type(CodeGen* gen, const char* name);
int         rc_is_tracked(CodeGen* gen, const char* name);

// --- From codegen_expr.c: expression emission ---
void  emit_expr(CodeGen* gen, Node* node);
void  emit_struct_init(CodeGen* gen, Node* node);
void  emit_destruct_pattern(CodeGen* gen, DestructPattern* pattern, const char* temp_prefix,
                            int is_const);
Node* lookup_generic_template_field_type(CodeGen* gen, const char* field_name);
char* resolve_generic_field_dec_func(CodeGen* gen, Node* field_type_node);

// --- From codegen_stmt.c: statement emission ---
void emit_block_contents(CodeGen* gen, Node* block);

#endif
