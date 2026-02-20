#include "ast.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "vec.h"

// ============================================================================
// DestructPattern functions
// ============================================================================

DestructPattern* pattern_new_ident(const char* name, int length) {
    DestructPattern* pattern = xcalloc(1, sizeof(DestructPattern));
    pattern->kind            = PATTERN_IDENT;
    pattern->as.ident.name   = xmalloc(length + 1);
    memcpy(pattern->as.ident.name, name, length);
    pattern->as.ident.name[length] = '\0';
    pattern->as.ident.name_length  = length;
    pattern->resolved_type         = NULL;
    return pattern;
}

DestructPattern* pattern_new_tuple(int capacity) {
    DestructPattern* pattern   = xcalloc(1, sizeof(DestructPattern));
    pattern->kind              = PATTERN_TUPLE;
    pattern->as.tuple.elements = xmalloc(capacity * sizeof(DestructPattern*));
    pattern->as.tuple.count    = 0;
    pattern->resolved_type     = NULL;
    return pattern;
}

void pattern_tuple_push(DestructPattern* pattern, DestructPattern* elem) {
    // Note: caller must ensure capacity; for simplicity we just store
    pattern->as.tuple.elements[pattern->as.tuple.count++] = elem;
}

DestructPattern* pattern_new_struct(int capacity) {
    DestructPattern* pattern             = xcalloc(1, sizeof(DestructPattern));
    pattern->kind                        = PATTERN_STRUCT;
    pattern->as.struc.field_names        = xmalloc(capacity * sizeof(char*));
    pattern->as.struc.field_name_lengths = xmalloc(capacity * sizeof(int));
    pattern->as.struc.local_names        = xmalloc(capacity * sizeof(char*));
    pattern->as.struc.local_name_lengths = xmalloc(capacity * sizeof(int));
    pattern->as.struc.field_types        = xcalloc(capacity, sizeof(Type*));
    pattern->as.struc.count              = 0;
    pattern->as.struc.capacity           = capacity;
    pattern->resolved_type               = NULL;
    return pattern;
}

void pattern_struct_push(DestructPattern* pattern, const char* name, int length,
                         const char* local_name, int local_length) {
    if (pattern->as.struc.count >= pattern->as.struc.capacity) {
        pattern->as.struc.capacity *= 2;
        pattern->as.struc.field_names =
            xrealloc(pattern->as.struc.field_names, pattern->as.struc.capacity * sizeof(char*));
        pattern->as.struc.field_name_lengths = xrealloc(pattern->as.struc.field_name_lengths,
                                                        pattern->as.struc.capacity * sizeof(int));
        pattern->as.struc.local_names =
            xrealloc(pattern->as.struc.local_names, pattern->as.struc.capacity * sizeof(char*));
        pattern->as.struc.local_name_lengths = xrealloc(pattern->as.struc.local_name_lengths,
                                                        pattern->as.struc.capacity * sizeof(int));
        pattern->as.struc.field_types =
            xrealloc(pattern->as.struc.field_types, pattern->as.struc.capacity * sizeof(Type*));
    }
    int i                            = pattern->as.struc.count++;
    pattern->as.struc.field_names[i] = xmalloc(length + 1);
    memcpy(pattern->as.struc.field_names[i], name, length);
    pattern->as.struc.field_names[i][length] = '\0';
    pattern->as.struc.field_name_lengths[i]  = length;

    pattern->as.struc.local_names[i] = xmalloc(local_length + 1);
    memcpy(pattern->as.struc.local_names[i], local_name, local_length);
    pattern->as.struc.local_names[i][local_length] = '\0';
    pattern->as.struc.local_name_lengths[i]        = local_length;
}

void pattern_free(DestructPattern* pattern) {
    if (!pattern)
        return;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        free(pattern->as.ident.name);
        break;
    case PATTERN_TUPLE:
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            pattern_free(pattern->as.tuple.elements[i]);
        }
        free(pattern->as.tuple.elements);
        break;
    case PATTERN_STRUCT:
        for (int i = 0; i < pattern->as.struc.count; i++) {
            free(pattern->as.struc.field_names[i]);
            free(pattern->as.struc.local_names[i]);
        }
        free(pattern->as.struc.field_names);
        free(pattern->as.struc.field_name_lengths);
        free(pattern->as.struc.local_names);
        free(pattern->as.struc.local_name_lengths);
        free(pattern->as.struc.field_types);
        break;
    }
    free(pattern);
}

DestructPattern* pattern_clone(DestructPattern* pattern) {
    if (!pattern)
        return NULL;

    DestructPattern* c = xmalloc(sizeof(DestructPattern));
    c->kind            = pattern->kind;
    c->resolved_type   = NULL; // checker-set, will be re-populated

    switch (pattern->kind) {
    case PATTERN_IDENT:
        c->as.ident.name        = xstrdup(pattern->as.ident.name);
        c->as.ident.name_length = pattern->as.ident.name_length;
        break;
    case PATTERN_TUPLE:
        c->as.tuple.count    = pattern->as.tuple.count;
        c->as.tuple.elements = xmalloc(sizeof(DestructPattern*) * pattern->as.tuple.count);
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            c->as.tuple.elements[i] = pattern_clone(pattern->as.tuple.elements[i]);
        }
        break;
    case PATTERN_STRUCT:
        c->as.struc.count              = pattern->as.struc.count;
        c->as.struc.capacity           = pattern->as.struc.count;
        c->as.struc.field_names        = xmalloc(sizeof(char*) * pattern->as.struc.count);
        c->as.struc.field_name_lengths = xmalloc(sizeof(int) * pattern->as.struc.count);
        c->as.struc.local_names        = xmalloc(sizeof(char*) * pattern->as.struc.count);
        c->as.struc.local_name_lengths = xmalloc(sizeof(int) * pattern->as.struc.count);
        c->as.struc.field_types        = NULL; // checker-set, will be re-populated
        for (int i = 0; i < pattern->as.struc.count; i++) {
            c->as.struc.field_names[i]        = xstrdup(pattern->as.struc.field_names[i]);
            c->as.struc.field_name_lengths[i] = pattern->as.struc.field_name_lengths[i];
            c->as.struc.local_names[i]        = xstrdup(pattern->as.struc.local_names[i]);
            c->as.struc.local_name_lengths[i] = pattern->as.struc.local_name_lengths[i];
        }
        break;
    }
    return c;
}

// ============================================================================
// Node functions
// ============================================================================

Node* node_new(NodeType type, int line, int column) {
    Node* node   = xcalloc(1, sizeof(Node));
    node->type   = type;
    node->line   = line;
    node->column = column;
    return node;
}

void nodelist_init(NodeList* list) {
    list->nodes    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void nodelist_push(NodeList* list, Node* node) {
    VEC_GROW(list->nodes, list->count, list->capacity);
    list->nodes[list->count++] = node;
}

void nodelist_free(NodeList* list) {
    for (int i = 0; i < list->count; i++) {
        node_free(list->nodes[i]);
    }
    free(list->nodes);
    list->nodes    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

int match_stmt_has_wildcard_arm(Node* node) {
    if (!node || node->type != NODE_MATCH) {
        return 0;
    }
    for (int i = 0; i < node->as.match_stmt.arms.count; i++) {
        Node* arm = node->as.match_stmt.arms.nodes[i];
        if (arm && arm->type == NODE_MATCH_ARM && arm->as.match_arm.is_wildcard) {
            return 1;
        }
    }
    return 0;
}

void node_free(Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_STRING_LIT:
        free(node->as.string_lit.value);
        break;
    case NODE_IDENT:
        free(node->as.ident.name);
        break;
    case NODE_ENUM_VALUE:
        free(node->as.enum_value.enum_name);
        free(node->as.enum_value.value_name);
        nodelist_free(&node->as.enum_value.args);
        break;
    case NODE_ENUM_VARIANT:
        free(node->as.enum_variant.name);
        nodelist_free(&node->as.enum_variant.types);
        break;
    case NODE_NEW_EXPR:
        node_free(node->as.new_expr.type_node);
        node_free(node->as.new_expr.init);
        nodelist_free(&node->as.new_expr.args);
        break;
    case NODE_STRING_INTERP:
        nodelist_free(&node->as.string_interp.parts);
        free(node->as.string_interp.part_types);
        break;
    case NODE_CAST:
        node_free(node->as.cast_expr.expr);
        node_free(node->as.cast_expr.type_node);
        break;
    case NODE_TRY_EXPR:
        node_free(node->as.try_expr.expr);
        free(node->as.try_expr.enum_name);
        free(node->as.try_expr.ret_enum_name);
        break;
    case NODE_LAMBDA:
        nodelist_free(&node->as.lambda.params);
        node_free(node->as.lambda.return_type);
        node_free(node->as.lambda.body);
        for (int i = 0; i < node->as.lambda.captures.count; i++) {
            free(node->as.lambda.captures.names[i]);
        }
        free(node->as.lambda.captures.names);
        free(node->as.lambda.captures.types);
        free(node->as.lambda.captures.is_rc);
        break;
    case NODE_BINARY:
        node_free(node->as.binary.left);
        node_free(node->as.binary.right);
        break;
    case NODE_UNARY:
        node_free(node->as.unary.operand);
        break;
    case NODE_CALL:
        node_free(node->as.call.func);
        nodelist_free(&node->as.call.args);
        free(node->as.call.resolved_name);
        break;
    case NODE_INDEX:
        node_free(node->as.index.object);
        node_free(node->as.index.index);
        break;
    case NODE_ARRAY_TYPE:
        node_free(node->as.array_type.elem_type);
        node_free(node->as.array_type.size);
        break;
    case NODE_SLICE:
        node_free(node->as.slice.object);
        node_free(node->as.slice.start);
        node_free(node->as.slice.end);
        break;
    case NODE_MEMBER:
        node_free(node->as.member.object);
        free(node->as.member.name);
        free(node->as.member.struct_name);
        free(node->as.member.module_name);
        break;
    case NODE_ASSIGN:
        node_free(node->as.assign.target);
        node_free(node->as.assign.value);
        break;
    case NODE_STRUCT_INIT:
        nodelist_free(&node->as.struct_init.fields);
        break;
    case NODE_FIELD_INIT:
        free(node->as.field_init.name);
        node_free(node->as.field_init.value);
        break;
    case NODE_EXPR_STMT:
        node_free(node->as.expr_stmt.expr);
        break;
    case NODE_VAR_DECL:
        free(node->as.var_decl.name);
        node_free(node->as.var_decl.type);
        node_free(node->as.var_decl.init);
        // Free destructuring pattern if present
        pattern_free(node->as.var_decl.destruct_pattern);
        break;
    case NODE_BLOCK:
        nodelist_free(&node->as.block.stmts);
        break;
    case NODE_IF:
        node_free(node->as.if_stmt.cond);
        node_free(node->as.if_stmt.then_block);
        node_free(node->as.if_stmt.else_block);
        break;
    case NODE_IF_LET:
        node_free(node->as.if_let_stmt.expr);
        free(node->as.if_let_stmt.variant_name);
        free(node->as.if_let_stmt.enum_name);
        for (int i = 0; i < node->as.if_let_stmt.binding_count; i++) {
            free(node->as.if_let_stmt.bindings[i]);
        }
        free(node->as.if_let_stmt.bindings);
        node_free(node->as.if_let_stmt.then_block);
        node_free(node->as.if_let_stmt.else_block);
        node_free(node->as.if_let_stmt.extra_cond);
        break;
    case NODE_WHILE:
        node_free(node->as.while_stmt.cond);
        node_free(node->as.while_stmt.body);
        break;
    case NODE_FOR:
        node_free(node->as.for_stmt.init);
        node_free(node->as.for_stmt.cond);
        node_free(node->as.for_stmt.post);
        node_free(node->as.for_stmt.body);
        break;
    case NODE_FOREACH:
        free(node->as.foreach_stmt.var_name);
        node_free(node->as.foreach_stmt.start);
        node_free(node->as.foreach_stmt.end);
        node_free(node->as.foreach_stmt.step);
        node_free(node->as.foreach_stmt.body);
        node_free(node->as.foreach_stmt.collection);
        break;
    case NODE_RETURN:
        node_free(node->as.return_stmt.value);
        break;
    case NODE_DEFER:
        node_free(node->as.defer_stmt.stmt);
        break;
    case NODE_MATCH:
        node_free(node->as.match_stmt.expr);
        nodelist_free(&node->as.match_stmt.arms);
        break;
    case NODE_MATCH_ARM:
        free(node->as.match_arm.enum_name);
        free(node->as.match_arm.variant_name);
        for (int i = 0; i < node->as.match_arm.binding_count; i++) {
            free(node->as.match_arm.bindings[i]);
        }
        free(node->as.match_arm.bindings);
        node_free(node->as.match_arm.body);
        node_free(node->as.match_arm.pattern_expr);
        break;
    case NODE_FUNC_DECL:
        free(node->as.func_decl.receiver_type);
        // Free receiver type args if present
        nodelist_free(&node->as.func_decl.receiver_type_args);
        free(node->as.func_decl.name);
        free(node->as.func_decl.extern_name);
        nodelist_free(&node->as.func_decl.params);
        node_free(node->as.func_decl.return_type);
        node_free(node->as.func_decl.body);
        // Free type parameters and bounds for generic free functions
        for (int i = 0; i < node->as.func_decl.type_param_count; i++) {
            free(node->as.func_decl.type_params[i]);
            free(node->as.func_decl.type_param_bounds[i]);
        }
        free(node->as.func_decl.type_params);
        free(node->as.func_decl.type_param_bounds);
        // Free accessible modules list
        for (int i = 0; i < node->as.func_decl.accessible_modules_count; i++) {
            free(node->as.func_decl.accessible_modules[i]);
        }
        free(node->as.func_decl.accessible_modules);
        break;
    case NODE_PARAM:
        free(node->as.param.name);
        node_free(node->as.param.type);
        break;
    case NODE_STRUCT_DECL:
        free(node->as.struct_decl.name);
        nodelist_free(&node->as.struct_decl.fields);
        // Free type parameters and bounds if present
        for (int i = 0; i < node->as.struct_decl.type_param_count; i++) {
            free(node->as.struct_decl.type_params[i]);
            free(node->as.struct_decl.type_param_bounds[i]);
        }
        free(node->as.struct_decl.type_params);
        free(node->as.struct_decl.type_param_bounds);
        break;
    case NODE_FIELD:
        free(node->as.field.name);
        node_free(node->as.field.type);
        break;
    case NODE_ENUM_DECL:
        free(node->as.enum_decl.name);
        nodelist_free(&node->as.enum_decl.values);
        // Free type parameters and bounds if present
        for (int i = 0; i < node->as.enum_decl.type_param_count; i++) {
            free(node->as.enum_decl.type_params[i]);
            free(node->as.enum_decl.type_param_bounds[i]);
        }
        free(node->as.enum_decl.type_params);
        free(node->as.enum_decl.type_param_bounds);
        break;
    case NODE_TUPLE_TYPE:
        nodelist_free(&node->as.tuple_type.elem_types);
        break;
    case NODE_TUPLE_LIT:
        nodelist_free(&node->as.tuple_lit.elements);
        break;
    case NODE_ARRAY_LIT:
        nodelist_free(&node->as.array_lit.elements);
        break;
    case NODE_GENERIC_TYPE:
        free(node->as.generic_type.base_name);
        nodelist_free(&node->as.generic_type.type_args);
        break;
    case NODE_FUNC_TYPE:
        nodelist_free(&node->as.func_type.param_types);
        node_free(node->as.func_type.return_type);
        break;
    case NODE_PROGRAM:
        nodelist_free(&node->as.program.modules);
        break;
    case NODE_MODULE:
        free(node->as.module.name);
        nodelist_free(&node->as.module.decls);
        break;
    case NODE_TRAIT_DECL:
        free(node->as.trait_decl.name);
        nodelist_free(&node->as.trait_decl.methods);
        break;
    case NODE_IMPL_DECL:
        free(node->as.impl_decl.trait_name);
        free(node->as.impl_decl.type_name);
        // type_args nodes are shared with method receiver_type_args — just free the array
        free(node->as.impl_decl.type_args.nodes);
        nodelist_free(&node->as.impl_decl.methods);
        break;
    case NODE_TYPE_ALIAS:
        free(node->as.type_alias.name);
        node_free(node->as.type_alias.target_type);
        for (int i = 0; i < node->as.type_alias.type_param_count; i++) {
            free(node->as.type_alias.type_params[i]);
            free(node->as.type_alias.type_param_bounds[i]);
        }
        free(node->as.type_alias.type_params);
        free(node->as.type_alias.type_param_bounds);
        break;
    case NODE_USE_DECL:
        free(node->as.use_decl.module_name);
        for (int i = 0; i < node->as.use_decl.symbol_count; i++) {
            free(node->as.use_decl.symbol_names[i]);
        }
        free(node->as.use_decl.symbol_names);
        free(node->as.use_decl.symbol_name_lengths);
        break;
    case NODE_TEST_DECL:
        free(node->as.test_decl.name);
        node_free(node->as.test_decl.body);
        break;
    case NODE_EXTERN_MODULE:
        nodelist_free(&node->as.extern_module.decls);
        free(node->as.extern_module.module_name);
        break;
    default:
        break;
    }
    free(node);
}

// Deep clone a NodeList
static NodeList nodelist_clone(NodeList* list) {
    NodeList result;
    nodelist_init(&result);
    for (int i = 0; i < list->count; i++) {
        nodelist_push(&result, node_clone(list->nodes[i]));
    }
    return result;
}

// Reset all checker-set flags on a node to their initial state.
// Called after cloning to ensure each generic instantiation starts with clean metadata.
//
// MAINTENANCE: When adding a new "// Set by checker" field to ast.h, add the
// corresponding reset here. This is the ONLY place checker flags are cleared for clones.
// All fields listed here correspond to "Set by checker" comments in ast.h.
static void node_reset_checker_flags(Node* node) {
    // Top-level flags (apply to all expression node types)
    node->is_owned_temp   = 0;
    node->owned_temp_type = NULL;
    node->is_box_deref    = 0;

    switch (node->type) {
    case NODE_BINARY:
        node->as.binary.is_string_op = 0;
        node->as.binary.is_eq_op     = 0;
        node->as.binary.is_enum_eq   = 0;
        node->as.binary.eq_type_name = NULL;
        break;
    case NODE_INDEX:
        node->as.index.is_tuple_index = 0;
        node->as.index.is_span_index  = 0;
        node->as.index.is_vec_index   = 0;
        node->as.index.is_rc_elem     = 0;
        break;
    case NODE_SLICE:
        node->as.slice.resolved_type = NULL;
        node->as.slice.is_array      = 0;
        node->as.slice.is_vec        = 0;
        node->as.slice.is_string     = 0;
        break;
    case NODE_MEMBER:
        node->as.member.is_ref          = 0;
        node->as.member.is_const_access = 0;
        node->as.member.struct_name     = NULL;
        node->as.member.module_name     = NULL;
        node->as.member.is_method_ref   = 0;
        break;
    case NODE_ENUM_VALUE:
        node->as.enum_value.is_data_enum   = 0;
        node->as.enum_value.is_module_call = 0;
        break;
    case NODE_NEW_EXPR:
        node->as.new_expr.resolved_type = NULL;
        break;
    case NODE_STRING_INTERP:
        node->as.string_interp.part_types = NULL;
        break;
    case NODE_CAST:
        node->as.cast_expr.resolved_type = NULL;
        break;
    case NODE_TRY_EXPR:
        node->as.try_expr.resolved_type  = NULL;
        node->as.try_expr.unwrapped_type = NULL;
        node->as.try_expr.is_option      = 0;
        node->as.try_expr.enum_name      = NULL;
        node->as.try_expr.ret_enum_name  = NULL;
        break;
    case NODE_ARRAY_LIT:
        node->as.array_lit.resolved_type = NULL;
        break;
    case NODE_VAR_DECL:
        node->as.var_decl.is_rc         = 0;
        node->as.var_decl.resolved_type = NULL;
        break;
    case NODE_CALL:
        free(node->as.call.resolved_name);
        node->as.call.resolved_name      = NULL;
        node->as.call.is_indirect_call   = 0;
        node->as.call.resolved_func_type = NULL;
        break;
    case NODE_IDENT:
        node->as.ident.resolved_func_type = NULL;
        break;
    case NODE_FOREACH:
        node->as.foreach_stmt.resolved_type = NULL;
        node->as.foreach_stmt.is_span       = 0;
        node->as.foreach_stmt.is_string     = 0;
        break;
    case NODE_MATCH:
        node->as.match_stmt.resolved_type       = NULL;
        node->as.match_stmt.resolved_value_type = NULL;
        node->as.match_stmt.is_value_match      = 0;
        break;
    case NODE_IF_LET:
        node->as.if_let_stmt.resolved_type = NULL;
        break;
    case NODE_LAMBDA:
        node->as.lambda.lambda_id     = 0;
        node->as.lambda.resolved_type = NULL;
        for (int i = 0; i < node->as.lambda.captures.count; i++) {
            free(node->as.lambda.captures.names[i]);
        }
        free(node->as.lambda.captures.names);
        free(node->as.lambda.captures.types);
        free(node->as.lambda.captures.is_rc);
        node->as.lambda.captures.names    = NULL;
        node->as.lambda.captures.types    = NULL;
        node->as.lambda.captures.is_rc    = NULL;
        node->as.lambda.captures.count    = 0;
        node->as.lambda.captures.capacity = 0;
        break;
    default:
        break; // Node types without checker flags need no reset
    }
}

// Deep clone an AST node and all its children.
// Used to give each generic instantiation its own copy of method bodies,
// preventing checker-set flags from one instantiation affecting another.
//
// Checker-set flags are NOT copied here — they are zeroed by xcalloc and then
// explicitly reset by node_reset_checker_flags() after the structural copy.
// The checker will re-populate them when it checks the cloned body.
Node* node_clone(Node* node) {
    if (!node)
        return NULL;

    Node* c   = xcalloc(1, sizeof(Node));
    c->type   = node->type;
    c->line   = node->line;
    c->column = node->column;

    switch (node->type) {
    case NODE_INT_LIT:
        c->as.int_lit.value = node->as.int_lit.value;
        break;
    case NODE_FLOAT_LIT:
        c->as.float_lit.value = node->as.float_lit.value;
        break;
    case NODE_STRING_LIT:
        c->as.string_lit.value  = xstrdup(node->as.string_lit.value);
        c->as.string_lit.length = node->as.string_lit.length;
        break;
    case NODE_CHAR_LIT:
        c->as.char_lit.value = node->as.char_lit.value;
        break;
    case NODE_BOOL_LIT:
        c->as.bool_lit.value = node->as.bool_lit.value;
        break;
    case NODE_NULL_LIT:
        break;
    case NODE_IDENT:
        c->as.ident.name   = xstrdup(node->as.ident.name);
        c->as.ident.length = node->as.ident.length;
        break;
    case NODE_BINARY:
        c->as.binary.op    = node->as.binary.op;
        c->as.binary.left  = node_clone(node->as.binary.left);
        c->as.binary.right = node_clone(node->as.binary.right);
        break;
    case NODE_UNARY:
        c->as.unary.op      = node->as.unary.op;
        c->as.unary.operand = node_clone(node->as.unary.operand);
        break;
    case NODE_CALL:
        c->as.call.func = node_clone(node->as.call.func);
        c->as.call.args = nodelist_clone(&node->as.call.args);
        break;
    case NODE_INDEX:
        c->as.index.object = node_clone(node->as.index.object);
        c->as.index.index  = node_clone(node->as.index.index);
        break;
    case NODE_SLICE:
        c->as.slice.object = node_clone(node->as.slice.object);
        c->as.slice.start  = node_clone(node->as.slice.start);
        c->as.slice.end    = node_clone(node->as.slice.end);
        break;
    case NODE_MEMBER:
        c->as.member.object = node_clone(node->as.member.object);
        c->as.member.name   = xstrdup(node->as.member.name);
        c->as.member.length = node->as.member.length;
        break;
    case NODE_ASSIGN:
        c->as.assign.op     = node->as.assign.op;
        c->as.assign.target = node_clone(node->as.assign.target);
        c->as.assign.value  = node_clone(node->as.assign.value);
        break;
    case NODE_STRUCT_INIT:
        c->as.struct_init.fields = nodelist_clone(&node->as.struct_init.fields);
        break;
    case NODE_FIELD_INIT:
        c->as.field_init.name = node->as.field_init.name ? xstrdup(node->as.field_init.name) : NULL;
        c->as.field_init.name_length = node->as.field_init.name_length;
        c->as.field_init.value       = node_clone(node->as.field_init.value);
        break;
    case NODE_ENUM_VALUE:
        c->as.enum_value.enum_name         = xstrdup(node->as.enum_value.enum_name);
        c->as.enum_value.enum_name_length  = node->as.enum_value.enum_name_length;
        c->as.enum_value.value_name        = xstrdup(node->as.enum_value.value_name);
        c->as.enum_value.value_name_length = node->as.enum_value.value_name_length;
        c->as.enum_value.args              = nodelist_clone(&node->as.enum_value.args);
        break;
    case NODE_NEW_EXPR:
        c->as.new_expr.type_node = node_clone(node->as.new_expr.type_node);
        c->as.new_expr.init      = node_clone(node->as.new_expr.init);
        c->as.new_expr.args      = nodelist_clone(&node->as.new_expr.args);
        break;
    case NODE_STRING_INTERP:
        c->as.string_interp.parts      = nodelist_clone(&node->as.string_interp.parts);
        c->as.string_interp.part_count = node->as.string_interp.part_count;
        break;
    case NODE_CAST:
        c->as.cast_expr.expr      = node_clone(node->as.cast_expr.expr);
        c->as.cast_expr.type_node = node_clone(node->as.cast_expr.type_node);
        break;
    case NODE_TRY_EXPR:
        c->as.try_expr.expr = node_clone(node->as.try_expr.expr);
        break;
    case NODE_LAMBDA:
        c->as.lambda.params       = nodelist_clone(&node->as.lambda.params);
        c->as.lambda.return_type  = node_clone(node->as.lambda.return_type);
        c->as.lambda.body         = node_clone(node->as.lambda.body);
        c->as.lambda.is_expr_body = node->as.lambda.is_expr_body;
        break;
    case NODE_TUPLE_TYPE:
        c->as.tuple_type.elem_types = nodelist_clone(&node->as.tuple_type.elem_types);
        break;
    case NODE_TUPLE_LIT:
        c->as.tuple_lit.elements      = nodelist_clone(&node->as.tuple_lit.elements);
        c->as.tuple_lit.resolved_type = NULL; // Reset checker flag
        break;
    case NODE_ARRAY_LIT:
        c->as.array_lit.elements = nodelist_clone(&node->as.array_lit.elements);
        break;
    case NODE_ARRAY_TYPE:
        c->as.array_type.elem_type = node_clone(node->as.array_type.elem_type);
        c->as.array_type.size      = node_clone(node->as.array_type.size);
        break;
    case NODE_GENERIC_TYPE:
        c->as.generic_type.base_name        = xstrdup(node->as.generic_type.base_name);
        c->as.generic_type.base_name_length = node->as.generic_type.base_name_length;
        c->as.generic_type.type_args        = nodelist_clone(&node->as.generic_type.type_args);
        break;
    case NODE_FUNC_TYPE:
        c->as.func_type.param_types = nodelist_clone(&node->as.func_type.param_types);
        c->as.func_type.return_type = node_clone(node->as.func_type.return_type);
        break;
    case NODE_EXPR_STMT:
        c->as.expr_stmt.expr = node_clone(node->as.expr_stmt.expr);
        break;
    case NODE_VAR_DECL:
        c->as.var_decl.name = node->as.var_decl.name ? xstrdup(node->as.var_decl.name) : NULL;
        c->as.var_decl.name_length      = node->as.var_decl.name_length;
        c->as.var_decl.type             = node_clone(node->as.var_decl.type);
        c->as.var_decl.init             = node_clone(node->as.var_decl.init);
        c->as.var_decl.is_const         = node->as.var_decl.is_const;
        c->as.var_decl.destruct_pattern = pattern_clone(node->as.var_decl.destruct_pattern);
        break;
    case NODE_BLOCK:
        c->as.block.stmts = nodelist_clone(&node->as.block.stmts);
        break;
    case NODE_IF:
        c->as.if_stmt.cond       = node_clone(node->as.if_stmt.cond);
        c->as.if_stmt.then_block = node_clone(node->as.if_stmt.then_block);
        c->as.if_stmt.else_block = node_clone(node->as.if_stmt.else_block);
        break;
    case NODE_IF_LET:
        c->as.if_let_stmt.expr                = node_clone(node->as.if_let_stmt.expr);
        c->as.if_let_stmt.variant_name        = xstrdup(node->as.if_let_stmt.variant_name);
        c->as.if_let_stmt.variant_name_length = node->as.if_let_stmt.variant_name_length;
        c->as.if_let_stmt.enum_name =
            node->as.if_let_stmt.enum_name ? xstrdup(node->as.if_let_stmt.enum_name) : NULL;
        c->as.if_let_stmt.enum_name_length = node->as.if_let_stmt.enum_name_length;
        c->as.if_let_stmt.binding_count    = node->as.if_let_stmt.binding_count;
        if (node->as.if_let_stmt.binding_count > 0) {
            c->as.if_let_stmt.bindings =
                xmalloc(node->as.if_let_stmt.binding_count * sizeof(char*));
            for (int i = 0; i < node->as.if_let_stmt.binding_count; i++) {
                c->as.if_let_stmt.bindings[i] = xstrdup(node->as.if_let_stmt.bindings[i]);
            }
        }
        c->as.if_let_stmt.then_block = node_clone(node->as.if_let_stmt.then_block);
        c->as.if_let_stmt.else_block = node_clone(node->as.if_let_stmt.else_block);
        c->as.if_let_stmt.extra_cond = node_clone(node->as.if_let_stmt.extra_cond);
        break;
    case NODE_WHILE:
        c->as.while_stmt.cond = node_clone(node->as.while_stmt.cond);
        c->as.while_stmt.body = node_clone(node->as.while_stmt.body);
        break;
    case NODE_FOR:
        c->as.for_stmt.init = node_clone(node->as.for_stmt.init);
        c->as.for_stmt.cond = node_clone(node->as.for_stmt.cond);
        c->as.for_stmt.post = node_clone(node->as.for_stmt.post);
        c->as.for_stmt.body = node_clone(node->as.for_stmt.body);
        break;
    case NODE_FOREACH:
        c->as.foreach_stmt.var_name        = xstrdup(node->as.foreach_stmt.var_name);
        c->as.foreach_stmt.var_name_length = node->as.foreach_stmt.var_name_length;
        c->as.foreach_stmt.start           = node_clone(node->as.foreach_stmt.start);
        c->as.foreach_stmt.end             = node_clone(node->as.foreach_stmt.end);
        c->as.foreach_stmt.step            = node_clone(node->as.foreach_stmt.step);
        c->as.foreach_stmt.body            = node_clone(node->as.foreach_stmt.body);
        c->as.foreach_stmt.collection      = node_clone(node->as.foreach_stmt.collection);
        break;
    case NODE_RETURN:
        c->as.return_stmt.value = node_clone(node->as.return_stmt.value);
        break;
    case NODE_BREAK:
    case NODE_CONTINUE:
        break;
    case NODE_DEFER:
        c->as.defer_stmt.stmt = node_clone(node->as.defer_stmt.stmt);
        break;
    case NODE_MATCH:
        c->as.match_stmt.expr = node_clone(node->as.match_stmt.expr);
        c->as.match_stmt.arms = nodelist_clone(&node->as.match_stmt.arms);
        break;
    case NODE_MATCH_ARM:
        c->as.match_arm.enum_name =
            node->as.match_arm.enum_name ? xstrdup(node->as.match_arm.enum_name) : NULL;
        c->as.match_arm.enum_name_length = node->as.match_arm.enum_name_length;
        c->as.match_arm.variant_name =
            node->as.match_arm.variant_name ? xstrdup(node->as.match_arm.variant_name) : NULL;
        c->as.match_arm.variant_name_length = node->as.match_arm.variant_name_length;
        c->as.match_arm.binding_count       = node->as.match_arm.binding_count;
        c->as.match_arm.is_wildcard         = node->as.match_arm.is_wildcard;
        c->as.match_arm.body                = node_clone(node->as.match_arm.body);
        c->as.match_arm.pattern_expr        = node_clone(node->as.match_arm.pattern_expr);
        if (node->as.match_arm.binding_count > 0) {
            c->as.match_arm.bindings = xmalloc(node->as.match_arm.binding_count * sizeof(char*));
            for (int i = 0; i < node->as.match_arm.binding_count; i++) {
                c->as.match_arm.bindings[i] = xstrdup(node->as.match_arm.bindings[i]);
            }
        }
        break;
    case NODE_TEST_DECL:
        c->as.test_decl.name        = xstrdup(node->as.test_decl.name);
        c->as.test_decl.name_length = node->as.test_decl.name_length;
        c->as.test_decl.body        = node_clone(node->as.test_decl.body);
        break;
    case NODE_PARAM:
        c->as.param.name        = xstrdup(node->as.param.name);
        c->as.param.name_length = node->as.param.name_length;
        c->as.param.type        = node_clone(node->as.param.type);
        c->as.param.is_const    = node->as.param.is_const;
        break;
    case NODE_FIELD:
        c->as.field.name        = xstrdup(node->as.field.name);
        c->as.field.name_length = node->as.field.name_length;
        c->as.field.type        = node_clone(node->as.field.type);
        c->as.field.is_const    = node->as.field.is_const;
        c->as.field.is_private  = node->as.field.is_private;
        break;
    default:
        fprintf(stderr, "node_clone: unhandled node type %d\n", node->type);
        abort();
    }

    node_reset_checker_flags(c);
    return c;
}

// ============================================================================
// AST query functions (pure Node* traversals, no codegen state)
// ============================================================================

// Find a generic struct declaration by name in the AST
Node* find_generic_struct_decl(Node* ast, const char* name) {
    if (!ast || ast->type != NODE_PROGRAM)
        return NULL;
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL && decl->as.struct_decl.type_param_count > 0 &&
                strcmp(decl->as.struct_decl.name, name) == 0) {
                return decl;
            }
        }
    }
    return NULL;
}

// Find a generic enum declaration by name in the AST
Node* find_generic_enum_decl(Node* ast, const char* name) {
    if (!ast || ast->type != NODE_PROGRAM)
        return NULL;
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_ENUM_DECL && decl->as.enum_decl.type_param_count > 0 &&
                strcmp(decl->as.enum_decl.name, name) == 0) {
                return decl;
            }
        }
    }
    return NULL;
}

// Find a generic free function declaration in the AST by name
Node* find_generic_func_decl(Node* ast, const char* name) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int d = 0; d < mod->as.module.decls.count; d++) {
            Node* decl = mod->as.module.decls.nodes[d];
            if (decl->type == NODE_FUNC_DECL && decl->as.func_decl.type_param_count > 0 &&
                strcmp(decl->as.func_decl.name, name) == 0) {
                return decl;
            }
        }
    }
    return NULL;
}

// Find a method-level generic function declaration in the AST.
// Matches func (ReceiverType<...>) MethodName<...>(...): ...
Node* find_generic_method_func_decl(Node* ast, const char* receiver_type, const char* method_name) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int d = 0; d < mod->as.module.decls.count; d++) {
            Node* decl = mod->as.module.decls.nodes[d];
            // Direct method: func (Vec<T>) map<K>(...) -> Vec<K>
            if (decl->type == NODE_FUNC_DECL && decl->as.func_decl.receiver_type != NULL &&
                decl->as.func_decl.type_param_count > 0 &&
                strcmp(decl->as.func_decl.receiver_type, receiver_type) == 0 &&
                strcmp(decl->as.func_decl.name, method_name) == 0) {
                return decl;
            }
            // Inside impl blocks
            if (decl->type == NODE_IMPL_DECL) {
                for (int j = 0; j < decl->as.impl_decl.methods.count; j++) {
                    Node* method = decl->as.impl_decl.methods.nodes[j];
                    if (method->type == NODE_FUNC_DECL &&
                        method->as.func_decl.receiver_type != NULL &&
                        method->as.func_decl.type_param_count > 0 &&
                        strcmp(method->as.func_decl.receiver_type, receiver_type) == 0 &&
                        strcmp(method->as.func_decl.name, method_name) == 0) {
                        return method;
                    }
                }
            }
        }
    }
    return NULL;
}

// Return whether a function decl is a direct generic-receiver method for `struct_name`.
static int is_collectible_generic_method(Node* decl, const char* struct_name) {
    return decl && decl->type == NODE_FUNC_DECL && decl->as.func_decl.body != NULL &&
           decl->as.func_decl.receiver_type != NULL &&
           decl->as.func_decl.receiver_type_args.count > 0 &&
           decl->as.func_decl.type_param_count == 0 &&
           strcmp(decl->as.func_decl.receiver_type, struct_name) == 0;
}

// Collect all generic methods for a given struct name
void collect_generic_methods(Node* ast, const char* struct_name, Node*** methods_out,
                             int* count_out) {
    *methods_out = NULL;
    *count_out   = 0;

    if (!ast || ast->type != NODE_PROGRAM)
        return;

    int    capacity = 0;
    Node** methods  = NULL;
    int    count    = 0;

    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            // Direct generic methods: func (Box<T>) get() -> T.
            // Excludes method-level generics (type_param_count > 0), which are handled
            // via GenericFuncInstance.
            if (is_collectible_generic_method(decl, struct_name)) {
                VEC_GROW(methods, count, capacity);
                methods[count++] = decl;
            }

            // Methods inside impl blocks: impl Drop for Box { func (Box<T>) drop() -> void }
            if (decl->type != NODE_IMPL_DECL) {
                continue;
            }
            for (int j = 0; j < decl->as.impl_decl.methods.count; j++) {
                Node* method = decl->as.impl_decl.methods.nodes[j];
                if (is_collectible_generic_method(method, struct_name)) {
                    VEC_GROW(methods, count, capacity);
                    methods[count++] = method;
                }
            }
        }
    }

    *methods_out = methods;
    *count_out   = count;
}

// ============================================================================
// Owned temp predicates (pure Node* checks)
// ============================================================================

// Forward declaration for mutual recursion
static int has_owned_temps_children(Node* node);

// Return whether a new-expression subtree contains any owned temps.
static int has_owned_temps_new_expr(Node* node) {
    if (node->as.new_expr.init) {
        for (int i = 0; i < node->as.new_expr.init->as.struct_init.fields.count; i++) {
            Node* field = node->as.new_expr.init->as.struct_init.fields.nodes[i];
            if (field && field->type == NODE_FIELD_INIT &&
                has_owned_temps(field->as.field_init.value)) {
                return 1;
            }
        }
    }
    for (int i = 0; i < node->as.new_expr.args.count; i++) {
        if (has_owned_temps(node->as.new_expr.args.nodes[i])) {
            return 1;
        }
    }
    return 0;
}

// Return whether any child expression for this node contains owned temps.
static int has_owned_temps_children(Node* node) {
    switch (node->type) {
    case NODE_CALL:
        if (has_owned_temps(node->as.call.func))
            return 1;
        for (int i = 0; i < node->as.call.args.count; i++) {
            if (has_owned_temps(node->as.call.args.nodes[i]))
                return 1;
        }
        return 0;
    case NODE_BINARY:
        return has_owned_temps(node->as.binary.left) || has_owned_temps(node->as.binary.right);
    case NODE_UNARY:
        return has_owned_temps(node->as.unary.operand);
    case NODE_MEMBER:
        return has_owned_temps(node->as.member.object);
    case NODE_INDEX:
        return has_owned_temps(node->as.index.object) || has_owned_temps(node->as.index.index);
    case NODE_SLICE:
        return has_owned_temps(node->as.slice.object) || has_owned_temps(node->as.slice.start) ||
               has_owned_temps(node->as.slice.end);
    case NODE_NEW_EXPR:
        return has_owned_temps_new_expr(node);
    case NODE_CAST:
        return has_owned_temps(node->as.cast_expr.expr);
    case NODE_STRING_INTERP:
        for (int i = 0; i < node->as.string_interp.part_count; i++) {
            if (has_owned_temps(node->as.string_interp.parts.nodes[i]))
                return 1;
        }
        return 0;
    case NODE_ASSIGN:
        return has_owned_temps(node->as.assign.target) || has_owned_temps(node->as.assign.value);
    case NODE_ENUM_VALUE:
        for (int i = 0; i < node->as.enum_value.args.count; i++) {
            if (has_owned_temps(node->as.enum_value.args.nodes[i]))
                return 1;
        }
        return 0;
    default:
        return 0;
    }
}

// Check if an expression tree contains any owned temps.
int has_owned_temps(Node* node) {
    if (!node)
        return 0;
    if (node->is_owned_temp)
        return 1;
    return has_owned_temps_children(node);
}
