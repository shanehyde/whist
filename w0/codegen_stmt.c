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

static const char* enum_value_resolved_name(CodeGen* gen, Node* enum_value) {
    const char* name = sem_info_get_enum_value_resolved_name(gen->checker.sem, enum_value);
    if (name) {
        return name;
    }
    if (enum_value->as.enum_value.enum_name) {
        return enum_value->as.enum_value.enum_name;
    }
    return "";
}

static int enum_value_resolved_name_length(CodeGen* gen, Node* enum_value) {
    const char* name = sem_info_get_enum_value_resolved_name(gen->checker.sem, enum_value);
    if (name) {
        return (int)strlen(name);
    }
    return enum_value->as.enum_value.enum_name ? enum_value->as.enum_value.enum_name_length : 0;
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

// Emit an expression statement (NODE_EXPR_STMT).
// Handles RC var reassignment, RC member assignment, Vec index assignment,
// and simple expression statements.
static void emit_expr_stmt(CodeGen* gen, Node* node) {
    Node* expr = node->as.expr_stmt.expr;

    // Handle RC reassignment: p = new_value
    // Must inc new value, dec old value, then assign
    if (expr->type == NODE_ASSIGN && expr->as.assign.op == TOK_EQ &&
        expr->as.assign.target->type == NODE_IDENT &&
        rc_is_tracked(gen, expr->as.assign.target->as.ident.name)) {
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
            return;
        }

        // Evaluate new value into a temp (in case it references the old value)
        int temp_id = gen->out.temp_count++;
        emit_indent(gen);
        emit(gen, "void* __rc_tmp%d = (void*)", temp_id);
        emit_expr(gen, expr->as.assign.value);
        emit(gen, ";\n");
        emit_indent(gen);
        emit(gen, "__rc_inc(__rc_tmp%d);\n", temp_id);
        emit_indent(gen);
        emit(gen, "%s(%s);\n", rc_get_dec_func(gen, var_name), var_name);
        emit_indent(gen);
        emit(gen, "%s = __rc_tmp%d;\n", var_name, temp_id);
        return;
    }
    // Handle RC member assignment: line1.start = z
    if (expr->type == NODE_ASSIGN && expr->as.assign.op == TOK_EQ &&
        expr->as.assign.target->type == NODE_MEMBER) {
        Node* member    = expr->as.assign.target;
        int   obj_is_rc = member->as.member.object->type == NODE_IDENT &&
                        rc_is_tracked(gen, member->as.member.object->as.ident.name);
        if (obj_is_rc) {
            const char* obj_name = member->as.member.object->as.ident.name;
            Type*       obj_type = rc_get_var_type(gen, obj_name);
            Type*       field_ty = NULL;
            if (obj_type && obj_type->kind == TYPE_STRUCT) {
                for (int f = 0; f < obj_type->as.struc.field_count; f++) {
                    if (strcmp(obj_type->as.struc.field_names[f], member->as.member.name) == 0) {
                        field_ty = obj_type->as.struc.field_types[f];
                        break;
                    }
                }
            }
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
                return;
            }

            int value_is_rc = (expr->as.assign.value->type == NODE_IDENT &&
                               rc_is_tracked(gen, expr->as.assign.value->as.ident.name)) ||
                              expr->as.assign.value->type == NODE_NEW_EXPR;
            if (value_is_rc && field_ty && field_ty->kind == TYPE_STRUCT) {
                // Determine the dec function for the field's type
                char*       member_dec_owned = (char*)get_dec_func_for_type(field_ty);
                const char* member_dec       = member_dec_owned;
                int         tmp              = gen->out.temp_count++;
                emit_indent(gen);
                emit(gen, "void* __rc_tmp%d = (void*)", tmp);
                emit_expr(gen, expr->as.assign.value);
                emit(gen, ";\n");
                emit_indent(gen);
                emit(gen, "__rc_inc(__rc_tmp%d);\n", tmp);
                emit_indent(gen);
                emit(gen, "%s(", member_dec);
                emit_expr(gen, member);
                emit(gen, ");\n");
                emit_indent(gen);
                emit_expr(gen, member);
                emit(gen, " = __rc_tmp%d;\n", tmp);
                free(member_dec_owned);
                return;
            }
        }
        // Handle self.field = new_value in method bodies (self is not RC-tracked)
        if (!obj_is_rc && member->as.member.object->type == NODE_IDENT &&
            strcmp(member->as.member.object->as.ident.name, "self") == 0) {
            int   value_is_rc = expr->as.assign.value->type == NODE_NEW_EXPR;
            char* dec_fn      = NULL;
            if (value_is_rc && gen->generics.tmpl) {
                // Generic method: look up field type from template
                Node* ftype = lookup_generic_template_field_type(gen, member->as.member.name);
                dec_fn      = resolve_generic_field_dec_func(gen, ftype);
            }
            if (dec_fn) {
                // Dec the old field value, then assign the new one
                emit_indent(gen);
                emit(gen, "%s(", dec_fn);
                emit_expr(gen, member);
                emit(gen, ");\n");
                emit_indent(gen);
                emit_expr(gen, member);
                emit(gen, " = ");
                emit_expr(gen, expr->as.assign.value);
                emit(gen, ";\n");
                free(dec_fn);
                return;
            }
        }
    }
    // Handle Vec index assignment: v[i] = x → bounds check + direct data write
    if (expr->type == NODE_ASSIGN && expr->as.assign.target->type == NODE_INDEX &&
        expr->as.assign.target->as.index.is_vec_index) {
        Node* idx_node = expr->as.assign.target;
        emit_indent(gen);
        emit(gen, "__w0_vec_check(");
        emit_expr(gen, idx_node->as.index.object);
        emit(gen, "->count, ");
        emit_expr(gen, idx_node->as.index.index);
        emit(gen, ", %d, %d);\n", idx_node->line, idx_node->column);
        emit_indent(gen);
        emit_expr(gen, idx_node->as.index.object);
        emit(gen, "->data[");
        emit_expr(gen, idx_node->as.index.index);
        emit(gen, "] %s ", assign_op_str(expr->as.assign.op));
        emit_expr(gen, expr->as.assign.value);
        emit(gen, ";\n");
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
    emit(gen, "__Vec_%s* %s = (__Vec_%s*)__rc_alloc(sizeof(__Vec_%s));\n", elem_tname,
         node->as.var_decl.name, elem_tname, elem_tname);
    emit_indent(gen);
    emit(gen, "%s->data = NULL; %s->count = 0; %s->capacity = 0;\n", node->as.var_decl.name,
         node->as.var_decl.name, node->as.var_decl.name);
    Node* init = node->as.var_decl.init->as.new_expr.init;
    for (int i = 0; i < init->as.struct_init.fields.count; i++) {
        Node* field = init->as.struct_init.fields.nodes[i];
        if (field && field->type == NODE_FIELD_INIT) {
            emit_indent(gen);
            emit(gen, "__Vec_%s_push(%s, ", elem_tname, node->as.var_decl.name);
            emit_expr(gen, field->as.field_init.value);
            emit(gen, ");\n");
        }
    }
    char dec_buf[256];
    snprintf(dec_buf, sizeof(dec_buf), "__rc_dec_Vec_%s", elem_tname);
    rc_push_var(gen, node->as.var_decl.name, dec_buf, rtype);
}

// Emit an RC-managed struct declaration: var p = new Point { x: 1, y: 2 }
static void emit_var_decl_rc_new_struct(CodeGen* gen, Node* node) {
    Type*       rtype = node->as.var_decl.init->as.new_expr.resolved_type;
    const char* tname = rtype->as.struc.name;
    emit_indent(gen);
    emit(gen, "%s* %s = (%s*)__rc_alloc(sizeof(%s));\n", tname, node->as.var_decl.name, tname,
         tname);
    emit_indent(gen);
    emit(gen, "*%s = (%s)", node->as.var_decl.name, tname);
    emit_struct_init(gen, node->as.var_decl.init->as.new_expr.init);
    emit(gen, ";\n");
    // Increment refcount for any RC-tracked idents stored in struct fields
    Node* rc_init = node->as.var_decl.init->as.new_expr.init;
    for (int i = 0; i < rc_init->as.struct_init.fields.count; i++) {
        Node* field = rc_init->as.struct_init.fields.nodes[i];
        if (field && field->type == NODE_FIELD_INIT &&
            field->as.field_init.value->type == NODE_IDENT &&
            rc_is_tracked(gen, field->as.field_init.value->as.ident.name)) {
            const char* vname  = field->as.field_init.value->as.ident.name;
            Type*       vtype  = rc_get_var_type(gen, vname);
            const char* inc_fn = get_inc_func_for_type(vtype);
            emit_indent(gen);
            emit(gen, "%s(%s);\n", inc_fn, vname);
            free((char*)inc_fn);
        }
    }
    const char* dec_fn = get_dec_func_for_type(rtype);
    rc_push_var(gen, node->as.var_decl.name, dec_fn, rtype);
    free((char*)dec_fn);
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
    // Function calls transfer ownership (rc already 1), no inc needed
    Type* rc_type = node->as.var_decl.resolved_type;
    int   skip_inc =
        node->as.var_decl.init->type == NODE_CALL ||
        (rc_type && rc_type->kind == TYPE_ENUM && node->as.var_decl.init->type == NODE_ENUM_VALUE);
    if (!skip_inc && rc_type) {
        const char* inc_fn = get_inc_func_for_type(rc_type);
        emit_indent(gen);
        emit(gen, "%s(%s);\n", inc_fn, node->as.var_decl.name);
        free((char*)inc_fn);
    }
    const char* dec_fn = get_dec_func_for_type(rc_type);
    rc_push_var(gen, node->as.var_decl.name, dec_fn, rc_type);
    free((char*)dec_fn);
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
            emit_resolved_type(gen, node->as.var_decl.resolved_type);
            emit(gen, " %s", node->as.var_decl.name);
        } else {
            emit(gen, "int64_t %s", node->as.var_decl.name);
        }
        break;
    }
}

// Emit a variable declaration statement (NODE_VAR_DECL).
// Handles destructuring, RC-managed declarations, type inference, and struct init.
static void emit_var_decl_stmt(CodeGen* gen, Node* node) {
    // Handle destructuring: var (a, b) = tuple; or var (a, (b, c)) = nested;
    DestructPattern* pattern = node->as.var_decl.destruct_pattern;
    if (pattern) {
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
        return;
    }

    // Handle RC-managed variable declarations
    if (node->as.var_decl.is_rc && node->as.var_decl.init) {
        if (node->as.var_decl.init->type == NODE_NEW_EXPR) {
            Type* rtype = node->as.var_decl.init->as.new_expr.resolved_type;
            if (rtype && rtype->kind == TYPE_VEC) {
                emit_var_decl_rc_new_vec(gen, node);
            } else {
                emit_var_decl_rc_new_struct(gen, node);
            }
        } else {
            emit_var_decl_rc_copy(gen, node);
        }
        return;
    }

    emit_indent(gen);
    if (node->as.var_decl.is_const) {
        emit(gen, "const ");
    }

    int struct_type = node->as.var_decl.type && is_struct_type(gen, node->as.var_decl.type);

    if (node->as.var_decl.type) {
        emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
    } else {
        emit_var_decl_inferred_type(gen, node);
    }

    if (node->as.var_decl.init) {
        if (struct_type && node->as.var_decl.init->type == NODE_NULL_LIT) {
            emit(gen, " = NULL");
        } else {
            emit(gen, " = ");
            emit_expr(gen, node->as.var_decl.init);
        }
    }
    emit(gen, ";\n");
}

// Emit a return statement (NODE_RETURN).
// Handles RC cleanup, defer integration, and return value evaluation.
static void emit_return_stmt(CodeGen* gen, Node* node) {
    // Determine if we're returning an RC var (skip it in cleanup)
    const char* skip_name = NULL;
    if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
        // Check if the returned identifier is an RC var
        const char* ret_name = node->as.return_stmt.value->as.ident.name;
        for (int i = 0; i < gen->rc.count; i++) {
            if (strcmp(gen->rc.vars[i].name, ret_name) == 0) {
                skip_name = ret_name;
                break;
            }
        }
    }

    if (gen->defer.count > 0) {
        // With defers: store value in __ret, cleanup RC, goto cleanup
        emit_indent(gen);
        if (node->as.return_stmt.value) {
            emit(gen, "__ret = ");
            emit_expr(gen, node->as.return_stmt.value);
            emit(gen, ";\n");
        }
        if (gen->rc.count > 0) {
            rc_cleanup_all(gen, skip_name);
        }
        emit_indent(gen);
        emit(gen, "goto __cleanup;\n");
    } else {
        // No defers: cleanup RC, then return
        if (gen->rc.count > 0) {
            if (node->as.return_stmt.value && !skip_name) {
                // Complex expression: evaluate to temp first
                emit_indent(gen);
                emit(gen, "typeof(");
                emit_expr(gen, node->as.return_stmt.value);
                emit(gen, ") __rc_ret = ");
                emit_expr(gen, node->as.return_stmt.value);
                emit(gen, ";\n");
                rc_cleanup_all(gen, NULL);
                emit_indent(gen);
                emit(gen, "return __rc_ret;\n");
            } else {
                rc_cleanup_all(gen, skip_name);
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

// Emit a match statement as an if/else-if chain over the enum tag
static void emit_match_stmt(CodeGen* gen, Node* node) {
    Type* enum_type = node->as.match_stmt.resolved_type;
    if (!enum_type)
        return;

    int         is_data   = enum_type->as.enm.has_data;
    const char* enum_name = enum_type->as.enm.name;

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
}

// Dispatch statement code generation based on node type
void emit_stmt(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT:
        emit_expr_stmt(gen, node);
        break;

    case NODE_VAR_DECL:
        emit_var_decl_stmt(gen, node);
        break;

    case NODE_BLOCK:
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
        break;

    case NODE_IF: {
        emit_indent(gen);
        int cond_has_parens = (node->as.if_stmt.cond->type == NODE_BINARY ||
                               node->as.if_stmt.cond->type == NODE_UNARY);
        if (cond_has_parens) {
            emit(gen, "if ");
            emit_expr(gen, node->as.if_stmt.cond);
            emit(gen, " {\n");
        } else {
            emit(gen, "if (");
            emit_expr(gen, node->as.if_stmt.cond);
            emit(gen, ") {\n");
        }
        gen->out.indent++;
        // Emit then block contents directly (it's already a block)
        if (node->as.if_stmt.then_block->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.if_stmt.then_block);
        } else {
            emit_stmt(gen, node->as.if_stmt.then_block);
        }
        gen->out.indent--;
        emit_indent(gen);
        emit(gen, "}");

        if (node->as.if_stmt.else_block) {
            if (node->as.if_stmt.else_block->type == NODE_IF) {
                // else if
                emit(gen, " else ");
                // Remove indent for else if
                gen->out.indent--;
                emit_stmt(gen, node->as.if_stmt.else_block);
                gen->out.indent++;
            } else {
                emit(gen, " else {\n");
                gen->out.indent++;
                if (node->as.if_stmt.else_block->type == NODE_BLOCK) {
                    emit_block_contents(gen, node->as.if_stmt.else_block);
                } else {
                    emit_stmt(gen, node->as.if_stmt.else_block);
                }
                gen->out.indent--;
                emit_indent(gen);
                emit(gen, "}\n");
            }
        } else {
            emit(gen, "\n");
        }
        break;
    }

    case NODE_WHILE: {
        emit_indent(gen);
        int wcond_has_parens = (node->as.while_stmt.cond->type == NODE_BINARY ||
                                node->as.while_stmt.cond->type == NODE_UNARY);
        if (wcond_has_parens) {
            emit(gen, "while ");
            emit_expr(gen, node->as.while_stmt.cond);
            emit(gen, " {\n");
        } else {
            emit(gen, "while (");
            emit_expr(gen, node->as.while_stmt.cond);
            emit(gen, ") {\n");
        }
        gen->out.indent++;
        if (node->as.while_stmt.body->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.while_stmt.body);
        } else {
            emit_stmt(gen, node->as.while_stmt.body);
        }
        gen->out.indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;
    }

    case NODE_FOR:
        emit_indent(gen);
        emit(gen, "for (");
        // Init
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
        // Cond
        if (node->as.for_stmt.cond) {
            emit_expr(gen, node->as.for_stmt.cond);
        }
        emit(gen, "; ");
        // Post
        if (node->as.for_stmt.post) {
            emit_expr(gen, node->as.for_stmt.post);
        }
        emit(gen, ") {\n");
        gen->out.indent++;
        if (node->as.for_stmt.body->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.for_stmt.body);
        } else {
            emit_stmt(gen, node->as.for_stmt.body);
        }
        gen->out.indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

    case NODE_FOREACH:
        if (node->as.foreach_stmt.collection) {
            int idx_id = gen->out.temp_count++;
            if (node->as.foreach_stmt.is_string) {
                // String foreach: foreach (const c in str)
                emit_indent(gen);
                emit(gen, "for (int64_t __foreach_%d = 0; __foreach_%d < (int64_t)strlen(", idx_id,
                     idx_id);
                emit_expr(gen, node->as.foreach_stmt.collection);
                emit(gen, "); __foreach_%d++) {\n", idx_id);
                gen->out.indent++;
                emit_indent(gen);
                emit(gen, "char %s = ", node->as.foreach_stmt.var_name);
                emit_expr(gen, node->as.foreach_stmt.collection);
                emit(gen, "[__foreach_%d];\n", idx_id);
            } else {
                // Collection foreach: foreach (const item in collection)
                // Vec uses -> (pointer), Span uses . (value type)
                const char* access = node->as.foreach_stmt.is_span ? "." : "->";
                emit_indent(gen);
                emit(gen, "for (int64_t __foreach_%d = 0; __foreach_%d < ", idx_id, idx_id);
                emit_expr(gen, node->as.foreach_stmt.collection);
                emit(gen, "%scount; __foreach_%d++) {\n", access, idx_id);
                gen->out.indent++;
                // Declare the loop variable from collection data[idx]
                emit_indent(gen);
                emit_resolved_type(gen, node->as.foreach_stmt.resolved_type
                                            ? node->as.foreach_stmt.resolved_type
                                            : type_int64);
                emit(gen, " %s = ", node->as.foreach_stmt.var_name);
                emit_expr(gen, node->as.foreach_stmt.collection);
                emit(gen, "%sdata[__foreach_%d];\n", access, idx_id);
            }
            if (node->as.foreach_stmt.body->type == NODE_BLOCK) {
                emit_block_contents(gen, node->as.foreach_stmt.body);
            } else {
                emit_stmt(gen, node->as.foreach_stmt.body);
            }
            gen->out.indent--;
            emit_indent(gen);
            emit(gen, "}\n");
        } else {
            // Range foreach: for (<type> var = start; var < end; var += step)
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
            if (node->as.foreach_stmt.body->type == NODE_BLOCK) {
                emit_block_contents(gen, node->as.foreach_stmt.body);
            } else {
                emit_stmt(gen, node->as.foreach_stmt.body);
            }
            gen->out.indent--;
            emit_indent(gen);
            emit(gen, "}\n");
        }
        break;
    case NODE_RETURN:
        emit_return_stmt(gen, node);
        break;

    case NODE_DEFER:
        // Don't emit anything here - just push to defer stack
        defer_push(gen, node->as.defer_stmt.stmt);
        break;

    case NODE_MATCH:
        emit_match_stmt(gen, node);
        break;

    case NODE_BREAK:
        rc_cleanup_scope(gen, gen->rc.depth);
        emit_indent(gen);
        emit(gen, "break;\n");
        break;

    case NODE_CONTINUE:
        rc_cleanup_scope(gen, gen->rc.depth);
        emit_indent(gen);
        emit(gen, "continue;\n");
        break;

    default:
        emit_indent(gen);
        emit(gen, "/* unknown stmt %d */;\n", node->type);
        break;
    }
}
