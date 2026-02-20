#ifndef WHIST_CODEGEN_INTERNAL_H
#define WHIST_CODEGEN_INTERNAL_H

#include "codegen_emit.h"
#include "codegen_types.h"
#include "print_ast.h"

// --- From codegen.c: generic substitution ---
Type*           subst_lookup(CodeGen* gen, const char* name);
int             lookup_string_lit(CodeGen* gen, const char* value, int length);
GenericFuncDef* lookup_generic_func_def_for_instance(CodeGen* gen, const char* base_name);
int  parse_method_key(const char* base_name, char* recv_out, int recv_size, char* method_out,
                      int method_size);
void register_thunk(CodeGen* gen, const char* c_name, Type* func_type);

// --- From codegen_emit.c: shared helpers ---
void        defer_push(CodeGen* gen, Node* node);
int         codegen_return_type_is_void(Node* return_type);
void        emit_func_return_type(CodeGen* gen, func_decl_node* fdn);
int         codegen_find_tuple_type_index(CodeGen* gen, Type* tuple_type);
const char* codegen_enum_value_resolved_name(CodeGen* gen, Node* enum_value);
int         codegen_enum_value_resolved_name_length(CodeGen* gen, Node* enum_value);
void        emit_value_match_cond(CodeGen* gen, int match_id, Node* pattern, Type* expr_type);
const char* binary_op_str(TokenType op);
const char* unary_op_str(TokenType op);
const char* assign_op_str(TokenType op);

// --- From codegen_rc.c: RC variable tracking ---
char*       get_cleanup_func_for_type(Type* t);
const char* get_inc_func_for_type(Type* t);
void        rc_push_var(CodeGen* gen, const char* name, Type* type);
void        rc_cleanup_scope(CodeGen* gen, int depth);
void        rc_cleanup_all(CodeGen* gen, const char* skip_name);
Type*       rc_get_var_type(CodeGen* gen, const char* name);
int         rc_is_tracked(CodeGen* gen, const char* name);

// --- From codegen_expr.c: expression emission ---
void  emit_expr(CodeGen* gen, Node* node);
void  emit_struct_init(CodeGen* gen, Node* node);
void  emit_hoisted_new_expr(CodeGen* gen, Node* node, const char* temp_name);
void  emit_hoisted_owned_temp(CodeGen* gen, Node* node, const char* temp_name);
void  emit_destruct_pattern(CodeGen* gen, DestructPattern* pattern, const char* temp_prefix,
                            int is_const);
Node* lookup_generic_template_field_type(CodeGen* gen, const char* field_name);

// --- From codegen_stmt.c: statement emission ---
void emit_block_contents(CodeGen* gen, Node* block);
int  hoist_owned_temps(CodeGen* gen, Node* expr);
void cleanup_owned_temps(CodeGen* gen, int saved_count);

#endif
