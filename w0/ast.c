#include "ast.h"

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
    }
    free(pattern);
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
        break;
    case NODE_RETURN:
        node_free(node->as.return_stmt.value);
        break;
    case NODE_DEFER:
        node_free(node->as.defer_stmt.stmt);
        break;
    case NODE_FUNC_DECL:
        free(node->as.func_decl.receiver_type);
        // Free receiver type args if present
        nodelist_free(&node->as.func_decl.receiver_type_args);
        free(node->as.func_decl.name);
        nodelist_free(&node->as.func_decl.params);
        node_free(node->as.func_decl.return_type);
        node_free(node->as.func_decl.body);
        break;
    case NODE_PARAM:
        free(node->as.param.name);
        node_free(node->as.param.type);
        break;
    case NODE_STRUCT_DECL:
        free(node->as.struct_decl.name);
        nodelist_free(&node->as.struct_decl.fields);
        // Free type parameters if present
        for (int i = 0; i < node->as.struct_decl.type_param_count; i++) {
            free(node->as.struct_decl.type_params[i]);
        }
        free(node->as.struct_decl.type_params);
        break;
    case NODE_FIELD:
        free(node->as.field.name);
        node_free(node->as.field.type);
        break;
    case NODE_ENUM_DECL:
        free(node->as.enum_decl.name);
        nodelist_free(&node->as.enum_decl.values);
        // Free type parameters if present
        for (int i = 0; i < node->as.enum_decl.type_param_count; i++) {
            free(node->as.enum_decl.type_params[i]);
        }
        free(node->as.enum_decl.type_params);
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
    case NODE_EXTERN_MODULE:
        nodelist_free(&node->as.extern_module.decls);
        free(node->as.extern_module.module_name);
        break;
    default:
        break;
    }
    free(node);
}
