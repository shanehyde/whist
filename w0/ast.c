#include "ast.h"
#include <stdlib.h>
#include <string.h>

Node* node_new(NodeType type, int line, int column) {
    Node* node   = calloc(1, sizeof(Node));
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
    if (list->count >= list->capacity) {
        int new_cap    = list->capacity == 0 ? 8 : list->capacity * 2;
        list->nodes    = realloc(list->nodes, new_cap * sizeof(Node*));
        list->capacity = new_cap;
    }
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
    case NODE_MEMBER:
        node_free(node->as.member.object);
        free(node->as.member.name);
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
        node_free(node->as.foreach_stmt.body);
        break;
    case NODE_RETURN:
        node_free(node->as.return_stmt.value);
        break;
    case NODE_FUNC_DECL:
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
        break;
    case NODE_FIELD:
        free(node->as.field.name);
        node_free(node->as.field.type);
        break;
    case NODE_ENUM_DECL:
        free(node->as.enum_decl.name);
        nodelist_free(&node->as.enum_decl.values);
        break;
    case NODE_PROGRAM:
        nodelist_free(&node->as.program.decls);
        break;
    default:
        break;
    }
    free(node);
}
