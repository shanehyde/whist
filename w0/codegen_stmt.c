#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_internal.h"
#include "sem_info.h"
#include "types.h"
#include "vec.h"

// Forward declarations for static helpers
static void emit_expr_stmt(CodeGen* gen, Node* node);
static void emit_var_decl_stmt(CodeGen* gen, Node* node);
static void emit_return_stmt(CodeGen* gen, Node* node);
static void emit_match_stmt(CodeGen* gen, Node* node);

// Walk a destructuring pattern and RC-track any identifiers with RC-managed types
static void rc_track_destruct_pattern(CodeGen* gen, DestructPattern* pattern) {
    if (!pattern)
        return;
    switch (pattern->kind) {
    case PATTERN_IDENT:
        if (type_is_rc_managed(pattern->resolved_type)) {
            rc_push_var(gen, pattern->as.ident.name, pattern->resolved_type);
        }
        break;
    case PATTERN_TUPLE:
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            rc_track_destruct_pattern(gen, pattern->as.tuple.elements[i]);
        }
        break;
    case PATTERN_STRUCT:
        // Struct destructuring is handled separately
        break;
    }
}

static const char* enum_value_resolved_name(CodeGen* gen, Node* enum_value) {
    const char* name = sem_info_get_enum_value_resolved_name(gen->checker.sem, enum_value,
                                                             enum_value->as.enum_value.enum_name);
    return name ? name : "";
}

static int enum_value_resolved_name_length(CodeGen* gen, Node* enum_value) {
    const char* name = sem_info_get_enum_value_resolved_name(gen->checker.sem, enum_value,
                                                             enum_value->as.enum_value.enum_name);
    return name ? (int)strlen(name) : 0;
}

// Emit statements within a block without emitting braces, but with RC scope handling
void emit_block_contents(CodeGen* gen, Node* block) {
    if (!block || block->type != NODE_BLOCK)
        return;

    gen->rc.depth++;
    for (int i = 0; i < block->as.block.stmts.count; i++) {
        emit_stmt(gen, block->as.block.stmts.nodes[i]);
    }
    rc_cleanup_scope(gen, gen->rc.depth);
    gen->rc.depth--;
}

static Type* find_struct_field_type(Type* struct_type, const char* field_name) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT) {
        return NULL;
    }
    for (int i = 0; i < struct_type->as.struc.field_count; i++) {
        if (strcmp(struct_type->as.struc.field_names[i], field_name) == 0) {
            return struct_type->as.struc.field_types[i];
        }
    }
    return NULL;
}

static void hoist_push_new_arg(CodeGen* gen, Node* arg, const char* temp_name) {
    if (gen->hoist.count >= gen->hoist.capacity) {
        int new_cap         = gen->hoist.capacity == 0 ? 8 : gen->hoist.capacity * 2;
        gen->hoist.nodes    = xrealloc(gen->hoist.nodes, new_cap * sizeof(Node*));
        gen->hoist.names    = xrealloc(gen->hoist.names, new_cap * sizeof(char*));
        gen->hoist.capacity = new_cap;
    }
    gen->hoist.nodes[gen->hoist.count] = arg;
    gen->hoist.names[gen->hoist.count] = xstrdup(temp_name);
    gen->hoist.count++;
}

// Recursively collect all is_owned_temp nodes in an expression tree (depth-first).
// Children are collected before parents so inner temps are evaluated first.
static void collect_owned_temps(Node* node, Node*** temps, int* count, int* cap) {
    if (!node)
        return;

    // Walk children first (evaluation order: inner before outer)
    switch (node->type) {
    case NODE_CALL:
        collect_owned_temps(node->as.call.func, temps, count, cap);
        for (int i = 0; i < node->as.call.args.count; i++) {
            collect_owned_temps(node->as.call.args.nodes[i], temps, count, cap);
        }
        break;
    case NODE_BINARY:
        collect_owned_temps(node->as.binary.left, temps, count, cap);
        collect_owned_temps(node->as.binary.right, temps, count, cap);
        break;
    case NODE_UNARY:
        collect_owned_temps(node->as.unary.operand, temps, count, cap);
        break;
    case NODE_MEMBER:
        collect_owned_temps(node->as.member.object, temps, count, cap);
        break;
    case NODE_INDEX:
        collect_owned_temps(node->as.index.object, temps, count, cap);
        collect_owned_temps(node->as.index.index, temps, count, cap);
        break;
    case NODE_SLICE:
        collect_owned_temps(node->as.slice.object, temps, count, cap);
        collect_owned_temps(node->as.slice.start, temps, count, cap);
        collect_owned_temps(node->as.slice.end, temps, count, cap);
        break;
    case NODE_NEW_EXPR:
        // Walk new expr args and field init values
        if (node->as.new_expr.init) {
            for (int i = 0; i < node->as.new_expr.init->as.struct_init.fields.count; i++) {
                Node* field = node->as.new_expr.init->as.struct_init.fields.nodes[i];
                if (field && field->type == NODE_FIELD_INIT) {
                    collect_owned_temps(field->as.field_init.value, temps, count, cap);
                }
            }
        }
        for (int i = 0; i < node->as.new_expr.args.count; i++) {
            collect_owned_temps(node->as.new_expr.args.nodes[i], temps, count, cap);
        }
        break;
    case NODE_CAST:
        collect_owned_temps(node->as.cast_expr.expr, temps, count, cap);
        break;
    case NODE_STRING_INTERP:
        for (int i = 0; i < node->as.string_interp.part_count; i++) {
            collect_owned_temps(node->as.string_interp.parts.nodes[i], temps, count, cap);
        }
        break;
    case NODE_ASSIGN:
        collect_owned_temps(node->as.assign.target, temps, count, cap);
        collect_owned_temps(node->as.assign.value, temps, count, cap);
        break;
    default:
        break;
    }

    // Then collect this node if it's an owned temp
    if (node->is_owned_temp) {
        VEC_GROW(*temps, *count, *cap);
        (*temps)[(*count)++] = node;
    }
}

// Emit __rc_dec for a hoisted owned temp, using the appropriate dec function for its type.
static void emit_owned_temp_dec(CodeGen* gen, Node* node, const char* name) {
    if (node->type == NODE_LAMBDA) {
        // Closure env: dec the env pointer inside the __Closure struct
        emit_indent(gen);
        emit(gen, "__rc_dec(%s.env);\n", name);
        return;
    }
    Type* t = node->owned_temp_type;
    if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
        emit_indent(gen);
        emit(gen, "__rc_dec_%s(%s);\n", t->as.enm.name, name);
    } else if (t && t->kind == TYPE_STRING) {
        emit_indent(gen);
        emit(gen, "__rc_dec((void*)%s);\n", name);
    } else {
        emit_indent(gen);
        emit(gen, "__rc_dec(%s);\n", name);
    }
}

// Hoist all owned temps in an expression tree. Returns the saved hoist count.
int hoist_owned_temps(CodeGen* gen, Node* expr) {
    Node** temps = NULL;
    int    count = 0;
    int    cap   = 0;
    collect_owned_temps(expr, &temps, &count, &cap);

    int saved = gen->hoist.count;
    for (int i = 0; i < count; i++) {
        int  id = gen->out.temp_count++;
        char name[32];
        snprintf(name, sizeof(name), "__rc_tmp%d", id);
        emit_hoisted_owned_temp(gen, temps[i], name);
        hoist_push_new_arg(gen, temps[i], name);
    }
    free(temps);
    return saved;
}

// Emit __rc_dec for all owned temps hoisted since saved_count, then restore hoist state.
void cleanup_owned_temps(CodeGen* gen, int saved_count) {
    for (int i = saved_count; i < gen->hoist.count; i++) {
        emit_owned_temp_dec(gen, gen->hoist.nodes[i], gen->hoist.names[i]);
        free(gen->hoist.names[i]);
    }
    gen->hoist.count = saved_count;
}

// True when the RHS is a closure variable reference (not a named function symbol).
// Copying from such an lvalue needs an env retain.
static int closure_ident_needs_env_inc(Node* expr) {
    return expr && expr->type == NODE_IDENT && !expr->as.ident.resolved_func_type;
}

static int emit_rc_ident_assign_stmt(CodeGen* gen, Node* expr) {
    if (expr->type != NODE_ASSIGN || expr->as.assign.op != TOK_EQ ||
        expr->as.assign.target->type != NODE_IDENT ||
        !rc_is_tracked(gen, expr->as.assign.target->as.ident.name)) {
        return 0;
    }

    const char* var_name = expr->as.assign.target->as.ident.name;
    Type*       var_type = rc_get_var_type(gen, var_name);
    if (var_type && var_type->kind == TYPE_ENUM && var_type->as.enm.has_rc_fields) {
        // Enum value reassignment: dec old payload, then assign, then inc if copying
        int temp_id = gen->out.temp_count++;
        emit_indent(gen);
        emit(gen, "%s __rc_tmp%d = ", var_type->as.enm.name, temp_id);
        emit_expr(gen, expr->as.assign.value);
        emit(gen, ";\n");

        int needs_inc = (expr->as.assign.value->type == NODE_IDENT &&
                         rc_is_tracked(gen, expr->as.assign.value->as.ident.name));
        if (needs_inc) {
            emit_indent(gen);
            emit(gen, "__rc_inc_%s(__rc_tmp%d);\n", var_type->as.enm.name, temp_id);
        }

        emit_indent(gen);
        emit(gen, "__rc_dec_%s(%s);\n", var_type->as.enm.name, var_name);
        emit_indent(gen);
        emit(gen, "%s = __rc_tmp%d;\n", var_name, temp_id);
        return 1;
    }

    // Evaluate new value into a temp (in case it references the old value)
    int temp_id = gen->out.temp_count++;
    emit_indent(gen);
    emit(gen, "void* __rc_tmp%d = (void*)", temp_id);
    emit_expr(gen, expr->as.assign.value);
    emit(gen, ";\n");
    // Only inc when borrowing from an existing reference (ident or vec index).
    // Fresh allocations (new, call, concat) already have rc=1 — inc would over-count.
    int needs_inc =
        (expr->as.assign.value->type == NODE_IDENT &&
         rc_is_tracked(gen, expr->as.assign.value->as.ident.name)) ||
        (expr->as.assign.value->type == NODE_INDEX && expr->as.assign.value->as.index.is_rc_elem);
    if (needs_inc) {
        emit_indent(gen);
        emit(gen, "__rc_inc(__rc_tmp%d);\n", temp_id);
    }
    emit_indent(gen);
    if (var_type && var_type->kind == TYPE_STRING) {
        emit(gen, "__rc_dec((void*)%s);\n", var_name);
    } else {
        emit(gen, "__rc_dec(%s);\n", var_name);
    }
    emit_indent(gen);
    emit(gen, "%s = __rc_tmp%d;\n", var_name, temp_id);
    return 1;
}

static int emit_rc_member_assign_stmt(CodeGen* gen, Node* expr) {
    if (expr->type != NODE_ASSIGN || expr->as.assign.op != TOK_EQ ||
        expr->as.assign.target->type != NODE_MEMBER) {
        return 0;
    }

    Node* member    = expr->as.assign.target;
    int   obj_is_rc = member->as.member.object->type == NODE_IDENT &&
                    rc_is_tracked(gen, member->as.member.object->as.ident.name);

    if (obj_is_rc) {
        const char* obj_name = member->as.member.object->as.ident.name;
        Type*       obj_type = rc_get_var_type(gen, obj_name);
        Type*       field_ty = find_struct_field_type(obj_type, member->as.member.name);

        if (field_ty && field_ty->kind == TYPE_ENUM && field_ty->as.enm.has_rc_fields) {
            int temp_id = gen->out.temp_count++;
            emit_indent(gen);
            emit(gen, "%s __rc_tmp%d = ", field_ty->as.enm.name, temp_id);
            emit_expr(gen, expr->as.assign.value);
            emit(gen, ";\n");

            int needs_inc = (expr->as.assign.value->type == NODE_IDENT &&
                             rc_is_tracked(gen, expr->as.assign.value->as.ident.name));
            if (needs_inc) {
                emit_indent(gen);
                emit(gen, "__rc_inc_%s(__rc_tmp%d);\n", field_ty->as.enm.name, temp_id);
            }

            emit_indent(gen);
            emit(gen, "__rc_dec_%s(", field_ty->as.enm.name);
            emit_expr(gen, member);
            emit(gen, ");\n");
            emit_indent(gen);
            emit_expr(gen, member);
            emit(gen, " = __rc_tmp%d;\n", temp_id);
            return 1;
        }

        int value_is_rc = (expr->as.assign.value->type == NODE_IDENT &&
                           rc_is_tracked(gen, expr->as.assign.value->as.ident.name)) ||
                          expr->as.assign.value->type == NODE_NEW_EXPR ||
                          (expr->as.assign.value->type == NODE_INDEX &&
                           expr->as.assign.value->as.index.is_rc_elem);
        // For string fields, any value assignment needs RC handling
        if (field_ty && field_ty->kind == TYPE_STRING) {
            value_is_rc = 1;
        }
        if (value_is_rc && field_ty &&
            (field_ty->kind == TYPE_STRUCT || field_ty->kind == TYPE_STRING ||
             field_ty->kind == TYPE_VEC || field_ty->kind == TYPE_STRINGBUILDER)) {
            int tmp = gen->out.temp_count++;
            emit_indent(gen);
            emit(gen, "void* __rc_tmp%d = (void*)", tmp);
            emit_expr(gen, expr->as.assign.value);
            emit(gen, ";\n");
            // Only inc when borrowing from an existing reference.
            // Fresh allocations (new_expr, calls) already have rc=1.
            int needs_inc = (expr->as.assign.value->type == NODE_IDENT &&
                             rc_is_tracked(gen, expr->as.assign.value->as.ident.name)) ||
                            (expr->as.assign.value->type == NODE_INDEX &&
                             expr->as.assign.value->as.index.is_rc_elem);
            if (needs_inc) {
                emit_indent(gen);
                emit(gen, "__rc_inc(__rc_tmp%d);\n", tmp);
            }
            emit_indent(gen);
            if (field_ty->kind == TYPE_STRING) {
                emit(gen, "__rc_dec((void*)");
            } else {
                emit(gen, "__rc_dec(");
            }
            emit_expr(gen, member);
            emit(gen, ");\n");
            emit_indent(gen);
            emit_expr(gen, member);
            emit(gen, " = __rc_tmp%d;\n", tmp);
            return 1;
        }
    }

    // Handle self.field = new_value in method bodies (self is not RC-tracked)
    if (!obj_is_rc && member->as.member.object->type == NODE_IDENT &&
        strcmp(member->as.member.object->as.ident.name, "self") == 0 &&
        expr->as.assign.value->type == NODE_NEW_EXPR) {
        // Dec the old field value, then assign the new one
        emit_indent(gen);
        emit(gen, "__rc_dec(");
        emit_expr(gen, member);
        emit(gen, ");\n");
        emit_indent(gen);
        emit_expr(gen, member);
        emit(gen, " = ");
        emit_expr(gen, expr->as.assign.value);
        emit(gen, ";\n");
        return 1;
    }

    return 0;
}

static int emit_vec_index_assign_stmt(CodeGen* gen, Node* expr) {
    if (expr->type != NODE_ASSIGN || expr->as.assign.target->type != NODE_INDEX ||
        !expr->as.assign.target->as.index.is_vec_index) {
        return 0;
    }

    Node* idx_node = expr->as.assign.target;
    emit_indent(gen);
    emit(gen, "__w0_vec_check(");
    emit_expr(gen, idx_node->as.index.object);
    emit(gen, "->count, ");
    emit_expr(gen, idx_node->as.index.index);
    emit(gen, ", %d, %d);\n", idx_node->line, idx_node->column);
    // RC handling: save new value to temp, inc, dec old, assign temp
    if (idx_node->as.index.is_rc_elem) {
        int tmp = gen->out.temp_count++;
        emit_indent(gen);
        emit(gen, "void* __rc_tmp%d = ", tmp);
        emit_expr(gen, expr->as.assign.value);
        emit(gen, ";\n");
        emit_indent(gen);
        emit(gen, "__rc_inc(__rc_tmp%d);\n", tmp);
        emit_indent(gen);
        emit(gen, "__rc_dec(");
        emit_expr(gen, idx_node->as.index.object);
        emit(gen, "->data[");
        emit_expr(gen, idx_node->as.index.index);
        emit(gen, "]);\n");
        emit_indent(gen);
        emit_expr(gen, idx_node->as.index.object);
        emit(gen, "->data[");
        emit_expr(gen, idx_node->as.index.index);
        emit(gen, "] = __rc_tmp%d;\n", tmp);
        return 1;
    }
    emit_indent(gen);
    emit_expr(gen, idx_node->as.index.object);
    emit(gen, "->data[");
    emit_expr(gen, idx_node->as.index.index);
    emit(gen, "] %s ", assign_op_str(expr->as.assign.op));
    emit_expr(gen, expr->as.assign.value);
    emit(gen, ";\n");
    return 1;
}

// Check if an expression tree contains any owned temps.
int has_owned_temps(Node* node) {
    if (!node)
        return 0;
    if (node->is_owned_temp)
        return 1;

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
        if (node->as.new_expr.init) {
            for (int i = 0; i < node->as.new_expr.init->as.struct_init.fields.count; i++) {
                Node* field = node->as.new_expr.init->as.struct_init.fields.nodes[i];
                if (field && field->type == NODE_FIELD_INIT &&
                    has_owned_temps(field->as.field_init.value))
                    return 1;
            }
        }
        for (int i = 0; i < node->as.new_expr.args.count; i++) {
            if (has_owned_temps(node->as.new_expr.args.nodes[i]))
                return 1;
        }
        return 0;
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
    default:
        return 0;
    }
}

// Emit an expression statement (NODE_EXPR_STMT).
// Handles RC var reassignment, RC member assignment, Vec index assignment,
// and owned temporaries that need cleanup.
static void emit_expr_stmt(CodeGen* gen, Node* node) {
    Node* expr = node->as.expr_stmt.expr;

    if (emit_rc_ident_assign_stmt(gen, expr) || emit_rc_member_assign_stmt(gen, expr) ||
        emit_vec_index_assign_stmt(gen, expr)) {
        return;
    }

    // Closure reassignment: f = new_closure — dec old env before assign
    if (expr->type == NODE_ASSIGN && expr->as.assign.op == TOK_EQ &&
        expr->as.assign.target->type == NODE_IDENT) {
        char env_name[256];
        snprintf(env_name, sizeof(env_name), "%s.env", expr->as.assign.target->as.ident.name);
        if (rc_is_tracked(gen, env_name)) {
            int temp_id   = gen->out.temp_count++;
            int needs_inc = closure_ident_needs_env_inc(expr->as.assign.value);
            emit_indent(gen);
            emit(gen, "__Closure __cl_tmp%d = ", temp_id);
            emit_expr(gen, expr->as.assign.value);
            emit(gen, ";\n");
            if (needs_inc) {
                emit_indent(gen);
                emit(gen, "__rc_inc(__cl_tmp%d.env);\n", temp_id);
            }
            emit_indent(gen);
            emit(gen, "__rc_dec(%s);\n", env_name);
            emit_indent(gen);
            emit(gen, "%s = __cl_tmp%d;\n", expr->as.assign.target->as.ident.name, temp_id);
            return;
        }
    }

    // Check for owned temps in the expression tree
    if (has_owned_temps(expr)) {
        int saved = hoist_owned_temps(gen, expr);
        emit_indent(gen);
        emit_expr(gen, expr);
        emit(gen, ";\n");
        cleanup_owned_temps(gen, saved);
        return;
    }

    emit_indent(gen);
    emit_expr(gen, expr);
    emit(gen, ";\n");
}

// Emit an RC-managed Vec declaration: var v = new Vec<T>{...}
static void emit_var_decl_rc_new_vec(CodeGen* gen, Node* node) {
    Type*       rtype      = node->as.var_decl.init->as.new_expr.resolved_type;
    const char* elem_tname = type_mangle_name(rtype->as.vec.elem);
    emit_indent(gen);
    emit(gen, "__Vec_%s* %s = (__Vec_%s*)__rc_alloc(sizeof(__Vec_%s), __Vec_%s_cleanup);\n",
         elem_tname, node->as.var_decl.name, elem_tname, elem_tname, elem_tname);
    emit_indent(gen);
    emit(gen, "%s->data = NULL; %s->count = 0; %s->capacity = 0;\n", node->as.var_decl.name,
         node->as.var_decl.name, node->as.var_decl.name);
    Node* init = node->as.var_decl.init->as.new_expr.init;
    for (int i = 0; i < init->as.struct_init.fields.count; i++) {
        Node* field = init->as.struct_init.fields.nodes[i];
        if (field && field->type == NODE_FIELD_INIT) {
            Node* val = field->as.field_init.value;
            if (val->is_owned_temp) {
                // Owned temp (new expr, call, etc.): hoist to temp, push, then dec.
                // push() does __rc_inc, so the temp's original ref must be released.
                int tmp = gen->out.temp_count++;
                emit_indent(gen);
                emit(gen, "void* __rc_tmp%d = (void*)", tmp);
                emit_expr(gen, val);
                emit(gen, ";\n");
                emit_indent(gen);
                emit(gen, "__Vec_%s_push(%s, __rc_tmp%d);\n", elem_tname, node->as.var_decl.name,
                     tmp);
                emit_indent(gen);
                emit(gen, "__rc_dec(__rc_tmp%d);\n", tmp);
            } else {
                emit_indent(gen);
                emit(gen, "__Vec_%s_push(%s, ", elem_tname, node->as.var_decl.name);
                emit_expr(gen, val);
                emit(gen, ");\n");
            }
        }
    }
    rc_push_var(gen, node->as.var_decl.name, rtype);
}

// Emit an RC-managed StringBuilder declaration: var sb = new StringBuilder{}
static void emit_var_decl_rc_new_stringbuilder(CodeGen* gen, Node* node) {
    emit_indent(gen);
    emit(gen,
         "__StringBuilder* %s = (__StringBuilder*)__rc_alloc(sizeof(__StringBuilder), "
         "__StringBuilder_cleanup);\n",
         node->as.var_decl.name);
    emit_indent(gen);
    emit(gen, "%s->data = NULL; %s->count = 0; %s->capacity = 0;\n", node->as.var_decl.name,
         node->as.var_decl.name, node->as.var_decl.name);
    Type* rtype = node->as.var_decl.init->as.new_expr.resolved_type;
    rc_push_var(gen, node->as.var_decl.name, rtype);
}

// Emit an RC-managed struct declaration with init call: var p = new Point(1, 2)
static void emit_var_decl_rc_new_init(CodeGen* gen, Node* node) {
    Type*       rtype   = node->as.var_decl.init->as.new_expr.resolved_type;
    const char* tname   = rtype->as.struc.name;
    char*       cleanup = get_cleanup_func_for_type(rtype);
    emit_indent(gen);
    emit(gen, "%s* %s = (%s*)__rc_alloc(sizeof(%s), %s);\n", tname, node->as.var_decl.name, tname,
         tname, cleanup ? cleanup : "NULL");
    free(cleanup);
    emit_indent(gen);
    emit(gen, "*%s = (%s){0};\n", node->as.var_decl.name, tname);
    emit_indent(gen);
    emit(gen, "%s_init(%s", tname, node->as.var_decl.name);
    Node* new_node = node->as.var_decl.init;
    for (int i = 0; i < new_node->as.new_expr.args.count; i++) {
        emit(gen, ", ");
        emit_expr(gen, new_node->as.new_expr.args.nodes[i]);
    }
    emit(gen, ");\n");
    // Increment refcount for any RC-tracked idents passed as args
    for (int i = 0; i < new_node->as.new_expr.args.count; i++) {
        Node* arg = new_node->as.new_expr.args.nodes[i];
        if (arg->type == NODE_IDENT && rc_is_tracked(gen, arg->as.ident.name)) {
            Type*       vtype  = rc_get_var_type(gen, arg->as.ident.name);
            const char* inc_fn = get_inc_func_for_type(vtype);
            emit_indent(gen);
            emit(gen, "%s(%s);\n", inc_fn, arg->as.ident.name);
            free((char*)inc_fn);
        }
    }
    rc_push_var(gen, node->as.var_decl.name, rtype);
}

// Emit an RC-managed struct declaration: var p = new Point { x: 1, y: 2 }
static void emit_var_decl_rc_new_struct(CodeGen* gen, Node* node) {
    Type*       rtype   = node->as.var_decl.init->as.new_expr.resolved_type;
    const char* tname   = rtype->as.struc.name;
    char*       cleanup = get_cleanup_func_for_type(rtype);
    emit_indent(gen);
    if (cleanup) {
        emit(gen, "%s* %s = (%s*)__rc_alloc(sizeof(%s), %s);\n", tname, node->as.var_decl.name,
             tname, tname, cleanup);
    } else {
        emit(gen, "%s* %s = (%s*)__rc_alloc(sizeof(%s), NULL);\n", tname, node->as.var_decl.name,
             tname, tname);
    }
    free(cleanup);
    emit_indent(gen);
    emit(gen, "*%s = (%s)", node->as.var_decl.name, tname);
    emit_struct_init(gen, node->as.var_decl.init->as.new_expr.init);
    emit(gen, ";\n");
    // Increment refcount for any RC values stored in struct fields
    Node* rc_init = node->as.var_decl.init->as.new_expr.init;
    for (int i = 0; i < rc_init->as.struct_init.fields.count; i++) {
        Node* field = rc_init->as.struct_init.fields.nodes[i];
        if (field && field->type == NODE_FIELD_INIT) {
            Node* val = field->as.field_init.value;
            if (val->type == NODE_IDENT && rc_is_tracked(gen, val->as.ident.name)) {
                const char* vname  = val->as.ident.name;
                Type*       vtype  = rc_get_var_type(gen, vname);
                const char* inc_fn = get_inc_func_for_type(vtype);
                emit_indent(gen);
                emit(gen, "%s(%s);\n", inc_fn, vname);
                free((char*)inc_fn);
            } else if (val->type == NODE_INDEX && val->as.index.is_rc_elem) {
                // Vec index read of RC element — inc the borrowed reference
                emit_indent(gen);
                emit(gen, "__rc_inc(%s->%s);\n", node->as.var_decl.name, field->as.field_init.name);
            }
        }
    }
    rc_push_var(gen, node->as.var_decl.name, rtype);
}

// Emit an RC copy or ownership transfer: var x = existing_rc_var or func_call()
static void emit_var_decl_rc_copy(CodeGen* gen, Node* node) {
    emit_indent(gen);
    if (node->as.var_decl.type) {
        emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
    } else if (node->as.var_decl.resolved_type) {
        Type* rtype = node->as.var_decl.resolved_type;
        if (rtype->kind == TYPE_STRUCT) {
            emit(gen, "%s* %s", rtype->as.struc.name, node->as.var_decl.name);
        } else {
            emit_resolved_type(gen, rtype);
            emit(gen, " %s", node->as.var_decl.name);
        }
    } else {
        emit(gen, "void* %s", node->as.var_decl.name);
    }
    emit(gen, " = ");
    emit_expr(gen, node->as.var_decl.init);
    emit(gen, ";\n");
    // Function calls and string operations transfer ownership (rc already 1), no inc needed
    Type* rc_type  = node->as.var_decl.resolved_type;
    Node* init     = node->as.var_decl.init;
    int   skip_inc = init->type == NODE_CALL || init->type == NODE_STRING_LIT ||
                   init->type == NODE_STRING_INTERP ||
                   (init->type == NODE_BINARY && init->as.binary.is_string_op) ||
                   (init->type == NODE_SLICE && init->as.slice.is_string) ||
                   (rc_type && rc_type->kind == TYPE_ENUM && init->type == NODE_ENUM_VALUE);
    if (!skip_inc && rc_type) {
        const char* inc_fn = get_inc_func_for_type(rc_type);
        emit_indent(gen);
        if (rc_type->kind == TYPE_STRING) {
            emit(gen, "%s((void*)%s);\n", inc_fn, node->as.var_decl.name);
        } else {
            emit(gen, "%s(%s);\n", inc_fn, node->as.var_decl.name);
        }
        free((char*)inc_fn);
    }
    rc_push_var(gen, node->as.var_decl.name, rc_type);
}

// Emit a type-and-name declaration inferred from the initializer expression.
static void emit_var_decl_inferred_type(CodeGen* gen, Node* node) {
    if (!node->as.var_decl.init) {
        emit(gen, "int64_t %s", node->as.var_decl.name);
        return;
    }
    switch (node->as.var_decl.init->type) {
    case NODE_INT_LIT:
        emit(gen, "int64_t %s", node->as.var_decl.name);
        break;
    case NODE_FLOAT_LIT:
        emit(gen, "float %s", node->as.var_decl.name);
        break;
    case NODE_BOOL_LIT:
        emit(gen, "bool %s", node->as.var_decl.name);
        break;
    case NODE_STRING_LIT:
        emit(gen, "const char* %s", node->as.var_decl.name);
        break;
    case NODE_CHAR_LIT:
        emit(gen, "char %s", node->as.var_decl.name);
        break;
    case NODE_TUPLE_LIT: {
        int    count = node->as.var_decl.init->as.tuple_lit.elements.count;
        Type** elems = xmalloc(count * sizeof(Type*));
        for (int i = 0; i < count; i++) {
            Node* elem = node->as.var_decl.init->as.tuple_lit.elements.nodes[i];
            switch (elem->type) {
            case NODE_INT_LIT:
                elems[i] = type_int64;
                break;
            case NODE_FLOAT_LIT:
                elems[i] = type_f32;
                break;
            case NODE_BOOL_LIT:
                elems[i] = type_bool;
                break;
            case NODE_STRING_LIT:
                elems[i] = type_string;
                break;
            case NODE_CHAR_LIT:
                elems[i] = type_char;
                break;
            case NODE_TUPLE_LIT:
                elems[i] = type_int64; // Fallback
                break;
            default:
                elems[i] = type_int64; // Default
                break;
            }
        }
        Type* tuple = type_tuple(elems, count);
        int   idx   = -1;
        for (int i = 0; i < gen->tuple_type_count; i++) {
            if (tuple_types_equal(gen->tuple_types[i], tuple)) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            emit(gen, "__tuple_t%d %s", idx, node->as.var_decl.name);
        } else {
            emit(gen, "struct { ");
            for (int i = 0; i < count; i++) {
                emit_resolved_type(gen, elems[i]);
                emit(gen, " _%d; ", i);
            }
            emit(gen, "} %s", node->as.var_decl.name);
        }
        break;
    }
    case NODE_ARRAY_LIT: {
        Node* init      = node->as.var_decl.init;
        Type* elem_type = init->as.array_lit.resolved_type;
        int   count     = init->as.array_lit.elements.count;
        emit_resolved_type(gen, elem_type);
        emit(gen, " %s[%d]", node->as.var_decl.name, count);
        break;
    }
    case NODE_ENUM_VALUE:
        emit(gen, "%.*s %s", enum_value_resolved_name_length(gen, node->as.var_decl.init),
             enum_value_resolved_name(gen, node->as.var_decl.init), node->as.var_decl.name);
        break;
    default:
        if (node->as.var_decl.resolved_type) {
            Type* rt = node->as.var_decl.resolved_type;
            if (rt->kind == TYPE_FUNC) {
                emit(gen, "__Closure %s", node->as.var_decl.name);
            } else {
                emit_resolved_type(gen, rt);
                emit(gen, " %s", node->as.var_decl.name);
            }
        } else {
            emit(gen, "int64_t %s", node->as.var_decl.name);
        }
        break;
    }
}

// Emit a variable declaration statement (NODE_VAR_DECL).
// Handles destructuring, RC-managed declarations, type inference, and struct init.
static void emit_var_decl_stmt(CodeGen* gen, Node* node) {
    // Handle destructuring: var (a, b) = tuple; or var {x, y} = struct_expr;
    DestructPattern* pattern = node->as.var_decl.destruct_pattern;
    if (pattern) {
        if (pattern->kind == PATTERN_STRUCT) {
            // Struct destructuring: var {a, b} = expr;
            Type* struct_type = pattern->resolved_type;
            int   temp_id     = gen->out.temp_count++;

            // Emit temp: StructName* __destruct0 = expr;
            // emit_resolved_type already adds * for struct types
            emit_indent(gen);
            emit_resolved_type(gen, struct_type);
            emit(gen, " __destruct%d = ", temp_id);
            emit_expr(gen, node->as.var_decl.init);
            emit(gen, ";\n");

            // Emit field extractions
            char temp_name[64];
            snprintf(temp_name, sizeof(temp_name), "__destruct%d", temp_id);
            emit_destruct_pattern(gen, pattern, temp_name, node->as.var_decl.is_const);

            // RC-track the temp (struct stays alive for scope)
            if (node->as.var_decl.is_rc) {
                rc_push_var(gen, temp_name, struct_type);
            }
            return;
        }
        // Tuple destructuring
        Type* tuple_type = pattern->resolved_type;
        emit_indent(gen);
        emit_resolved_type(gen, tuple_type);
        int temp_id = gen->out.temp_count++;
        emit(gen, " __tuple%d = ", temp_id);
        emit_expr(gen, node->as.var_decl.init);
        emit(gen, ";\n");
        char temp_prefix[64];
        snprintf(temp_prefix, sizeof(temp_prefix), "__tuple%d", temp_id);
        emit_destruct_pattern(gen, pattern, temp_prefix, node->as.var_decl.is_const);

        // RC-track any destructured variables with RC-managed types
        rc_track_destruct_pattern(gen, pattern);
        return;
    }

    // Handle RC-managed variable declarations
    if (node->as.var_decl.is_rc && node->as.var_decl.init) {
        // Direct RC assignment: ownership transfers to the variable, don't hoist the init itself
        node->as.var_decl.init->is_owned_temp = 0;
        if (node->as.var_decl.init->type == NODE_NEW_EXPR) {
            Type* rtype = node->as.var_decl.init->as.new_expr.resolved_type;
            if (rtype && rtype->kind == TYPE_VEC) {
                emit_var_decl_rc_new_vec(gen, node);
            } else if (rtype && rtype->kind == TYPE_STRINGBUILDER) {
                emit_var_decl_rc_new_stringbuilder(gen, node);
            } else if (node->as.var_decl.init->as.new_expr.init == NULL) {
                emit_var_decl_rc_new_init(gen, node);
            } else {
                emit_var_decl_rc_new_struct(gen, node);
            }
        } else {
            // Hoist nested owned temps (e.g., intermediate in nums.map(...).filter(...))
            int has_temps = has_owned_temps(node->as.var_decl.init);
            int saved     = 0;
            if (has_temps) {
                saved = hoist_owned_temps(gen, node->as.var_decl.init);
            }

            emit_var_decl_rc_copy(gen, node);

            if (has_temps) {
                cleanup_owned_temps(gen, saved);
            }
        }
        return;
    }

    // Direct lambda assignment: ownership transfers to variable's .env tracking, don't hoist
    int lambda_owned = 0;
    if (node->as.var_decl.init && node->as.var_decl.init->type == NODE_LAMBDA &&
        node->as.var_decl.init->is_owned_temp) {
        lambda_owned                          = 1;
        node->as.var_decl.init->is_owned_temp = 0;
    }

    // Hoist owned temps in init expression (e.g., var count = std.args().length)
    int saved     = 0;
    int has_temps = node->as.var_decl.init && has_owned_temps(node->as.var_decl.init);
    if (has_temps) {
        saved = hoist_owned_temps(gen, node->as.var_decl.init);
    }

    emit_indent(gen);
    if (node->as.var_decl.is_const) {
        emit(gen, "const ");
    }

    int struct_type = node->as.var_decl.type && is_struct_type(gen, node->as.var_decl.type);
    int is_func_var =
        (node->as.var_decl.resolved_type && node->as.var_decl.resolved_type->kind == TYPE_FUNC) ||
        (node->as.var_decl.type && node->as.var_decl.type->type == NODE_FUNC_TYPE);

    if (node->as.var_decl.type) {
        emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
    } else {
        emit_var_decl_inferred_type(gen, node);
    }

    if (node->as.var_decl.init) {
        if (struct_type && node->as.var_decl.init->type == NODE_NULL_LIT) {
            emit(gen, " = NULL");
        } else if (node->as.var_decl.init->type == NODE_NULL_LIT &&
                   ((node->as.var_decl.resolved_type &&
                     node->as.var_decl.resolved_type->kind == TYPE_FUNC) ||
                    (node->as.var_decl.type && node->as.var_decl.type->type == NODE_FUNC_TYPE))) {
            emit(gen, " = (__Closure){NULL, NULL}");
        } else {
            emit(gen, " = ");
            emit_expr(gen, node->as.var_decl.init);
        }
    } else if (is_func_var) {
        // Ensure env cleanup sees a deterministic pointer value.
        emit(gen, " = (__Closure){NULL, NULL}");
    }
    emit(gen, ";\n");

    // Closure copy from another closure variable needs an env retain.
    if (is_func_var && closure_ident_needs_env_inc(node->as.var_decl.init)) {
        emit_indent(gen);
        emit(gen, "__rc_inc(%s.env);\n", node->as.var_decl.name);
    }

    // Cleanup owned temps after the var decl
    if (has_temps) {
        cleanup_owned_temps(gen, saved);
    }

    // Restore lambda is_owned_temp flag
    if (lambda_owned) {
        node->as.var_decl.init->is_owned_temp = 1;
    }

    // Track closure env for scope cleanup: __rc_dec(f.env) at scope exit.
    // __rc_dec(NULL) is a no-op, so this is safe even for non-capturing closures.
    if (is_func_var) {
        char env_name[256];
        snprintf(env_name, sizeof(env_name), "%s.env", node->as.var_decl.name);
        rc_push_var(gen, env_name, NULL);
    }
}

// Emit a return statement (NODE_RETURN).
// Handles RC cleanup, defer integration, and return value evaluation.
static void emit_return_stmt(CodeGen* gen, Node* node) {
    // Determine if we're returning an RC var (skip it in cleanup)
    const char* skip_name    = NULL;
    char        env_buf[256] = {0};
    if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
        // Check if the returned identifier is an RC var
        const char* ret_name = node->as.return_stmt.value->as.ident.name;
        for (int i = 0; i < gen->rc.count; i++) {
            if (strcmp(gen->rc.vars[i].name, ret_name) == 0) {
                skip_name = ret_name;
                break;
            }
        }
        // If returning a closure variable, skip its .env cleanup (ownership transfer)
        if (!skip_name) {
            snprintf(env_buf, sizeof(env_buf), "%s.env", ret_name);
            if (rc_is_tracked(gen, env_buf)) {
                skip_name = env_buf;
            }
        }
    }

    // Handle owned temps in return expression.
    // If the return value itself is an owned temp (e.g., return new Box{...}),
    // ownership transfers to the caller — exclude it from hoisting.
    int has_temps    = 0;
    int saved        = 0;
    int ret_is_owned = 0;
    if (node->as.return_stmt.value) {
        ret_is_owned = node->as.return_stmt.value->is_owned_temp;
        if (ret_is_owned)
            node->as.return_stmt.value->is_owned_temp = 0;
        has_temps = has_owned_temps(node->as.return_stmt.value);
        if (has_temps)
            saved = hoist_owned_temps(gen, node->as.return_stmt.value);
        if (ret_is_owned)
            node->as.return_stmt.value->is_owned_temp = 1; // restore AST
    }

    if (gen->defer.count > 0) {
        // With defers: store value in __ret, cleanup RC + owned temps, goto cleanup
        emit_indent(gen);
        if (node->as.return_stmt.value) {
            emit(gen, "__ret = ");
            emit_expr(gen, node->as.return_stmt.value);
            emit(gen, ";\n");
        }
        if (gen->rc.count > 0) {
            rc_cleanup_all(gen, skip_name);
        }
        if (has_temps) {
            cleanup_owned_temps(gen, saved);
        }
        emit_indent(gen);
        emit(gen, "goto __cleanup;\n");
    } else {
        // No defers: cleanup RC + owned temps, then return
        if (gen->rc.count > 0 || has_temps) {
            if (node->as.return_stmt.value && !skip_name) {
                // Complex expression: evaluate to temp first
                emit_indent(gen);
                emit(gen, "typeof(");
                emit_expr(gen, node->as.return_stmt.value);
                emit(gen, ") __rc_ret = ");
                emit_expr(gen, node->as.return_stmt.value);
                emit(gen, ";\n");
                if (gen->rc.count > 0) {
                    rc_cleanup_all(gen, NULL);
                }
                if (has_temps) {
                    cleanup_owned_temps(gen, saved);
                }
                emit_indent(gen);
                emit(gen, "return __rc_ret;\n");
            } else {
                if (gen->rc.count > 0) {
                    rc_cleanup_all(gen, skip_name);
                }
                if (has_temps) {
                    cleanup_owned_temps(gen, saved);
                }
                emit_indent(gen);
                emit(gen, "return");
                if (node->as.return_stmt.value) {
                    emit(gen, " ");
                    emit_expr(gen, node->as.return_stmt.value);
                }
                emit(gen, ";\n");
            }
        } else {
            emit_indent(gen);
            emit(gen, "return");
            if (node->as.return_stmt.value) {
                emit(gen, " ");
                emit_expr(gen, node->as.return_stmt.value);
            }
            emit(gen, ";\n");
        }
    }
}

// Emit a comparison for a value match arm pattern
static void emit_value_match_cond(CodeGen* gen, int match_id, Node* pattern, Type* expr_type) {
    if (expr_type->kind == TYPE_STRING) {
        emit(gen, "strcmp(__match%d, ", match_id);
        emit_expr(gen, pattern);
        emit(gen, ") == 0");
    } else {
        emit(gen, "__match%d == ", match_id);
        emit_expr(gen, pattern);
    }
}

// Emit a value match statement (non-enum) as an if/else-if chain
static void emit_value_match_stmt(CodeGen* gen, Node* node) {
    Type* expr_type = node->as.match_stmt.resolved_type;
    if (!expr_type)
        return;

    // Handle owned temps in subject expression
    Node* subject          = node->as.match_stmt.expr;
    int   subject_is_owned = subject->is_owned_temp;
    if (subject_is_owned)
        subject->is_owned_temp =
            0; // exclude subject from hoisting (ownership transfers to __matchN)

    int saved     = 0;
    int has_temps = has_owned_temps(subject);
    if (has_temps) {
        saved = hoist_owned_temps(gen, subject);
    }

    int match_id = gen->out.temp_count++;
    emit_indent(gen);
    emit_resolved_type(gen, expr_type);
    emit(gen, " __match%d = ", match_id);
    emit_expr(gen, node->as.match_stmt.expr);
    emit(gen, ";\n");

    int first = 1;
    for (int a = 0; a < node->as.match_stmt.arms.count; a++) {
        Node* arm = node->as.match_stmt.arms.nodes[a];

        emit_indent(gen);
        if (arm->as.match_arm.is_wildcard) {
            if (first) {
                emit(gen, "{\n");
            } else {
                emit(gen, "else {\n");
            }
        } else {
            if (first) {
                emit(gen, "if (");
            } else {
                emit(gen, "else if (");
            }
            emit_value_match_cond(gen, match_id, arm->as.match_arm.pattern_expr, expr_type);
            emit(gen, ") {\n");
        }
        first = 0;

        gen->out.indent++;
        if (arm->as.match_arm.body) {
            if (arm->as.match_arm.body->type == NODE_BLOCK) {
                emit_block_contents(gen, arm->as.match_arm.body);
            } else {
                emit_stmt(gen, arm->as.match_arm.body);
            }
        }
        gen->out.indent--;
        emit_indent(gen);
        emit(gen, "}\n");
    }

    // Cleanup sub-expression owned temps
    if (has_temps) {
        cleanup_owned_temps(gen, saved);
    }
    // Cleanup subject if it was an owned temp (ownership was transferred to __matchN)
    if (subject_is_owned) {
        subject->is_owned_temp = 1; // restore AST
        char match_name[32];
        snprintf(match_name, sizeof(match_name), "__match%d", match_id);
        emit_owned_temp_dec(gen, subject, match_name);
    }
}

// Emit a match statement as an if/else-if chain over the enum tag
static void emit_match_stmt(CodeGen* gen, Node* node) {
    if (node->as.match_stmt.is_value_match) {
        emit_value_match_stmt(gen, node);
        return;
    }

    Type* enum_type = node->as.match_stmt.resolved_type;
    if (!enum_type)
        return;

    int         is_data   = enum_type->as.enm.has_data;
    const char* enum_name = enum_type->as.enm.name;

    // Handle owned temps in subject expression
    Node* subject          = node->as.match_stmt.expr;
    int   subject_is_owned = subject->is_owned_temp;
    if (subject_is_owned)
        subject->is_owned_temp =
            0; // exclude subject from hoisting (ownership transfers to __matchN)

    int saved     = 0;
    int has_temps = has_owned_temps(subject);
    if (has_temps) {
        saved = hoist_owned_temps(gen, subject);
    }

    // Emit temp variable: EnumType __matchN = <expr>;
    int match_id = gen->out.temp_count++;
    emit_indent(gen);
    emit(gen, "%s __match%d = ", enum_name, match_id);
    emit_expr(gen, node->as.match_stmt.expr);
    emit(gen, ";\n");

    // Check if exhaustive (no wildcard arm) — if so, emit last arm as 'else'
    // to suppress "control reaches end of non-void function" C compiler warnings
    int has_wildcard = 0;
    for (int a = 0; a < node->as.match_stmt.arms.count; a++) {
        if (node->as.match_stmt.arms.nodes[a]->as.match_arm.is_wildcard) {
            has_wildcard = 1;
            break;
        }
    }
    int last_arm = node->as.match_stmt.arms.count - 1;

    int first = 1;
    for (int a = 0; a < node->as.match_stmt.arms.count; a++) {
        Node* arm = node->as.match_stmt.arms.nodes[a];

        emit_indent(gen);
        if (arm->as.match_arm.is_wildcard) {
            if (first) {
                emit(gen, "{\n");
            } else {
                emit(gen, "else {\n");
            }
        } else if (!has_wildcard && a == last_arm && !first) {
            // Exhaustive match: emit last arm as 'else' so the C compiler
            // knows one branch always executes
            emit(gen, "else {\n");
        } else {
            const char* variant = arm->as.match_arm.variant_name;
            if (first) {
                emit(gen, "if (");
            } else {
                emit(gen, "else if (");
            }
            if (is_data) {
                emit(gen, "__match%d.tag == %s_%s", match_id, enum_name, variant);
            } else {
                emit(gen, "__match%d == %s_%s", match_id, enum_name, variant);
            }
            emit(gen, ") {\n");
        }
        first = 0;

        gen->out.indent++;

        // Emit binding declarations for data enum variants
        if (!arm->as.match_arm.is_wildcard && is_data && arm->as.match_arm.binding_count > 0) {
            const char* variant = arm->as.match_arm.variant_name;
            // Look up variant index to get types
            int variant_idx = -1;
            for (int i = 0; i < enum_type->as.enm.value_count; i++) {
                if (strcmp(enum_type->as.enm.value_names[i], variant) == 0) {
                    variant_idx = i;
                    break;
                }
            }
            for (int j = 0; j < arm->as.match_arm.binding_count; j++) {
                emit_indent(gen);
                emit_resolved_type(gen, enum_type->as.enm.variant_types[variant_idx][j]);
                emit(gen, " %s = __match%d.%s.f%d;\n", arm->as.match_arm.bindings[j], match_id,
                     variant, j);
            }
        }

        // Emit arm body
        if (arm->as.match_arm.body) {
            if (arm->as.match_arm.body->type == NODE_BLOCK) {
                emit_block_contents(gen, arm->as.match_arm.body);
            } else {
                emit_stmt(gen, arm->as.match_arm.body);
            }
        }

        gen->out.indent--;
        emit_indent(gen);
        emit(gen, "}\n");
    }

    // Cleanup sub-expression owned temps
    if (has_temps) {
        cleanup_owned_temps(gen, saved);
    }
    // Cleanup subject if it was an owned temp (ownership was transferred to __matchN)
    if (subject_is_owned) {
        subject->is_owned_temp = 1; // restore AST
        char match_name[32];
        snprintf(match_name, sizeof(match_name), "__match%d", match_id);
        emit_owned_temp_dec(gen, subject, match_name);
    }
}

static int stmt_cond_has_outer_parens(Node* cond) {
    return cond && (cond->type == NODE_BINARY || cond->type == NODE_UNARY);
}

static void emit_stmt_body(CodeGen* gen, Node* body) {
    if (!body)
        return;
    if (body->type == NODE_BLOCK) {
        emit_block_contents(gen, body);
    } else {
        emit_stmt(gen, body);
    }
}

static void emit_block_stmt(CodeGen* gen, Node* node) {
    emit_indent(gen);
    emit(gen, "{\n");
    gen->out.indent++;
    gen->rc.depth++;
    for (int i = 0; i < node->as.block.stmts.count; i++) {
        emit_stmt(gen, node->as.block.stmts.nodes[i]);
    }
    rc_cleanup_scope(gen, gen->rc.depth);
    gen->rc.depth--;
    gen->out.indent--;
    emit_indent(gen);
    emit(gen, "}\n");
}

static void emit_if_stmt(CodeGen* gen, Node* node) {
    // Hoist owned temps in condition before the if statement
    int has_temps = has_owned_temps(node->as.if_stmt.cond);
    int saved     = 0;
    if (has_temps) {
        saved = hoist_owned_temps(gen, node->as.if_stmt.cond);
    }

    emit_indent(gen);
    if (stmt_cond_has_outer_parens(node->as.if_stmt.cond)) {
        emit(gen, "if ");
        emit_expr(gen, node->as.if_stmt.cond);
        emit(gen, " {\n");
    } else {
        emit(gen, "if (");
        emit_expr(gen, node->as.if_stmt.cond);
        emit(gen, ") {\n");
    }

    gen->out.indent++;
    emit_stmt_body(gen, node->as.if_stmt.then_block);
    gen->out.indent--;

    emit_indent(gen);
    emit(gen, "}");

    if (node->as.if_stmt.else_block) {
        if (node->as.if_stmt.else_block->type == NODE_IF) {
            // Check if the else-if's condition has owned temps — if so, wrap in
            // braces so the hoist statements have a valid location
            int else_if_has_temps = has_owned_temps(node->as.if_stmt.else_block->as.if_stmt.cond);
            if (else_if_has_temps) {
                emit(gen, " else {\n");
                gen->out.indent++;
                emit_stmt(gen, node->as.if_stmt.else_block);
                gen->out.indent--;
                emit_indent(gen);
                emit(gen, "}\n");
            } else {
                emit(gen, " else ");
                gen->out.indent--;
                emit_stmt(gen, node->as.if_stmt.else_block);
                gen->out.indent++;
            }
        } else {
            emit(gen, " else {\n");
            gen->out.indent++;
            emit_stmt_body(gen, node->as.if_stmt.else_block);
            gen->out.indent--;
            emit_indent(gen);
            emit(gen, "}\n");
        }
    } else {
        emit(gen, "\n");
    }

    // Cleanup owned temps after the entire if/else chain
    if (has_temps) {
        cleanup_owned_temps(gen, saved);
    }
}

static void emit_while_stmt(CodeGen* gen, Node* node) {
    // If condition has owned temps, transform to for(;;) with hoist/cleanup each iteration
    if (has_owned_temps(node->as.while_stmt.cond)) {
        int cond_id = gen->out.temp_count++;
        emit_indent(gen);
        emit(gen, "for (;;) {\n");
        gen->out.indent++;

        // Hoist owned temps in condition
        int saved = hoist_owned_temps(gen, node->as.while_stmt.cond);

        // Evaluate condition to a bool temp
        emit_indent(gen);
        emit(gen, "bool __while_cond%d = ", cond_id);
        if (stmt_cond_has_outer_parens(node->as.while_stmt.cond)) {
            emit_expr(gen, node->as.while_stmt.cond);
        } else {
            emit(gen, "(");
            emit_expr(gen, node->as.while_stmt.cond);
            emit(gen, ")");
        }
        emit(gen, ";\n");

        // Cleanup owned temps
        cleanup_owned_temps(gen, saved);

        // Break if condition is false
        emit_indent(gen);
        emit(gen, "if (!__while_cond%d) break;\n", cond_id);

        // Emit loop body
        emit_stmt_body(gen, node->as.while_stmt.body);
        gen->out.indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        return;
    }

    emit_indent(gen);
    if (stmt_cond_has_outer_parens(node->as.while_stmt.cond)) {
        emit(gen, "while ");
        emit_expr(gen, node->as.while_stmt.cond);
        emit(gen, " {\n");
    } else {
        emit(gen, "while (");
        emit_expr(gen, node->as.while_stmt.cond);
        emit(gen, ") {\n");
    }

    gen->out.indent++;
    emit_stmt_body(gen, node->as.while_stmt.body);
    gen->out.indent--;
    emit_indent(gen);
    emit(gen, "}\n");
}

static void emit_for_stmt(CodeGen* gen, Node* node) {
    emit_indent(gen);
    emit(gen, "for (");

    if (node->as.for_stmt.init) {
        if (node->as.for_stmt.init->type == NODE_VAR_DECL) {
            Node* v = node->as.for_stmt.init;
            if (v->as.var_decl.type) {
                emit_type_with_name(gen, v->as.var_decl.type, v->as.var_decl.name);
            } else {
                emit(gen, "int64_t %s", v->as.var_decl.name);
            }
            if (v->as.var_decl.init) {
                emit(gen, " = ");
                emit_expr(gen, v->as.var_decl.init);
            }
        } else {
            emit_expr(gen, node->as.for_stmt.init);
        }
    }

    emit(gen, "; ");
    if (node->as.for_stmt.cond) {
        emit_expr(gen, node->as.for_stmt.cond);
    }
    emit(gen, "; ");
    if (node->as.for_stmt.post) {
        emit_expr(gen, node->as.for_stmt.post);
    }
    emit(gen, ") {\n");

    gen->out.indent++;
    emit_stmt_body(gen, node->as.for_stmt.body);
    gen->out.indent--;
    emit_indent(gen);
    emit(gen, "}\n");
}

static void emit_foreach_collection_stmt(CodeGen* gen, Node* node) {
    // Hoist owned temps in collection expression (evaluated once before the loop)
    int saved = 0;
    if (has_owned_temps(node->as.foreach_stmt.collection)) {
        saved = hoist_owned_temps(gen, node->as.foreach_stmt.collection);
    }

    int idx_id = gen->out.temp_count++;
    if (node->as.foreach_stmt.is_string) {
        emit_indent(gen);
        emit(gen, "for (int64_t __foreach_%d = 0; __foreach_%d < (int64_t)strlen(", idx_id, idx_id);
        emit_expr(gen, node->as.foreach_stmt.collection);
        emit(gen, "); __foreach_%d++) {\n", idx_id);
        gen->out.indent++;
        emit_indent(gen);
        emit(gen, "char %s = ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.collection);
        emit(gen, "[__foreach_%d];\n", idx_id);
    } else {
        const char* access = node->as.foreach_stmt.is_span ? "." : "->";
        emit_indent(gen);
        emit(gen, "for (int64_t __foreach_%d = 0; __foreach_%d < ", idx_id, idx_id);
        emit_expr(gen, node->as.foreach_stmt.collection);
        emit(gen, "%scount; __foreach_%d++) {\n", access, idx_id);
        gen->out.indent++;
        emit_indent(gen);
        emit_resolved_type(gen, node->as.foreach_stmt.resolved_type
                                    ? node->as.foreach_stmt.resolved_type
                                    : type_int64);
        emit(gen, " %s = ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.collection);
        emit(gen, "%sdata[__foreach_%d];\n", access, idx_id);
    }

    emit_stmt_body(gen, node->as.foreach_stmt.body);
    gen->out.indent--;
    emit_indent(gen);
    emit(gen, "}\n");

    // Cleanup owned temps after the loop
    if (saved != gen->hoist.count) {
        cleanup_owned_temps(gen, saved);
    }
}

static void emit_foreach_range_stmt(CodeGen* gen, Node* node) {
    emit_indent(gen);
    emit(gen, "for (");
    emit_resolved_type(gen, node->as.foreach_stmt.resolved_type
                                ? node->as.foreach_stmt.resolved_type
                                : type_int64);
    emit(gen, " %s = ", node->as.foreach_stmt.var_name);
    emit_expr(gen, node->as.foreach_stmt.start);
    emit(gen, "; %s < ", node->as.foreach_stmt.var_name);
    emit_expr(gen, node->as.foreach_stmt.end);
    emit(gen, "; %s += ", node->as.foreach_stmt.var_name);
    emit_expr(gen, node->as.foreach_stmt.step);
    emit(gen, ") {\n");
    gen->out.indent++;
    emit_stmt_body(gen, node->as.foreach_stmt.body);
    gen->out.indent--;
    emit_indent(gen);
    emit(gen, "}\n");
}

static void emit_foreach_stmt(CodeGen* gen, Node* node) {
    if (node->as.foreach_stmt.collection) {
        emit_foreach_collection_stmt(gen, node);
    } else {
        emit_foreach_range_stmt(gen, node);
    }
}

static void emit_defer_stmt(CodeGen* gen, Node* node) {
    defer_push(gen, node->as.defer_stmt.stmt);
}

static void emit_break_stmt(CodeGen* gen, Node* node) {
    (void)node;
    rc_cleanup_scope(gen, gen->rc.depth);
    emit_indent(gen);
    emit(gen, "break;\n");
}

static void emit_continue_stmt(CodeGen* gen, Node* node) {
    (void)node;
    rc_cleanup_scope(gen, gen->rc.depth);
    emit_indent(gen);
    emit(gen, "continue;\n");
}

static void emit_unknown_stmt(CodeGen* gen, Node* node) {
    emit_indent(gen);
    emit(gen, "/* unknown stmt %d */;\n", node ? node->type : -1);
}

typedef void (*StmtEmitter)(CodeGen* gen, Node* node);

static const StmtEmitter stmt_emitters[NODE_PROGRAM + 1] = {
    [NODE_EXPR_STMT] = emit_expr_stmt,  [NODE_VAR_DECL] = emit_var_decl_stmt,
    [NODE_BLOCK] = emit_block_stmt,     [NODE_IF] = emit_if_stmt,
    [NODE_WHILE] = emit_while_stmt,     [NODE_FOR] = emit_for_stmt,
    [NODE_FOREACH] = emit_foreach_stmt, [NODE_RETURN] = emit_return_stmt,
    [NODE_BREAK] = emit_break_stmt,     [NODE_CONTINUE] = emit_continue_stmt,
    [NODE_DEFER] = emit_defer_stmt,     [NODE_MATCH] = emit_match_stmt,
};

// Dispatch statement code generation based on node type
void emit_stmt(CodeGen* gen, Node* node) {
    if (!node) {
        return;
    }

    if (gen->line_directives && node->line > 0) {
        emit_indent(gen);
        emit(gen, "#line %d \"%s\"\n", node->line, gen->source_file);
    }

    if ((unsigned)node->type < (sizeof(stmt_emitters) / sizeof(stmt_emitters[0]))) {
        StmtEmitter emit_fn = stmt_emitters[node->type];
        if (emit_fn) {
            emit_fn(gen, node);
            return;
        }
    }

    emit_unknown_stmt(gen, node);
}
