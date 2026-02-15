#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_internal.h"
#include "sem_info.h"
#include "types.h"

// Forward declarations for static helpers
static void emit_string_lit(CodeGen* gen, Node* node);
static void emit_char_lit(CodeGen* gen, Node* node);
static void emit_enum_value(CodeGen* gen, Node* node);
static void emit_call_expr(CodeGen* gen, Node* node);
static void emit_index_expr(CodeGen* gen, Node* node);
static void emit_slice_expr(CodeGen* gen, Node* node);
static void emit_member_expr(CodeGen* gen, Node* node);
static void emit_new_expr(CodeGen* gen, Node* node);
static void emit_string_interp(CodeGen* gen, Node* node);
static void emit_match_expr(CodeGen* gen, Node* node);

static const char* member_struct_name(CodeGen* gen, Node* member) {
    return sem_info_get_member_struct_name(gen->checker.sem, member, member->as.member.struct_name);
}

static const char* member_module_name(CodeGen* gen, Node* member) {
    return sem_info_get_member_module_name(gen->checker.sem, member, member->as.member.module_name);
}

static int member_is_ref(CodeGen* gen, Node* member) {
    return sem_info_get_member_is_ref(gen->checker.sem, member, member->as.member.is_ref);
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

// In a generic method body, look up the field type node from the struct template.
// For `self.fieldname`, returns the AST type node of the field, or NULL.
Node* lookup_generic_template_field_type(CodeGen* gen, const char* field_name) {
    if (!gen->generics.tmpl)
        return NULL;
    Node* tmpl = gen->generics.tmpl;
    for (int i = 0; i < tmpl->as.struct_decl.fields.count; i++) {
        Node* field = tmpl->as.struct_decl.fields.nodes[i];
        if (strcmp(field->as.field.name, field_name) == 0) {
            return field->as.field.type;
        }
    }
    return NULL;
}

// Resolve the struct name for a method call in a generic method body.
// For `self.field.method(...)`, determines if `field` is a Vec/struct and returns
// the mangled name (e.g., "__Vec_HashEntry_i32" or "HashEntry_i32").
// Returns a malloc'd string, or NULL if unresolvable. Caller must free.
static char* resolve_generic_method_target(CodeGen* gen, Node* member) {
    if (!gen->generics.subst || !gen->generics.tmpl)
        return NULL;
    // Only handle `self.field.method()` pattern
    Node* obj = member->as.member.object;
    if (!obj || obj->type != NODE_MEMBER)
        return NULL;
    // obj should be `self.field`
    if (!obj->as.member.object || obj->as.member.object->type != NODE_IDENT)
        return NULL;
    if (strcmp(obj->as.member.object->as.ident.name, "self") != 0)
        return NULL;
    // Look up the field type in the template
    Node* field_type = lookup_generic_template_field_type(gen, obj->as.member.name);
    if (!field_type || field_type->type != NODE_GENERIC_TYPE)
        return NULL;
    const char* base = field_type->as.generic_type.base_name;
    if (strcmp(base, "Vec") == 0) {
        // Build "__Vec_" + mangled element type
        Node* elem = field_type->as.generic_type.type_args.nodes[0];
        char  buf[256];
        if (elem->type == NODE_IDENT) {
            const char* elem_name = elem->as.ident.name;
            Type*       resolved  = subst_lookup(gen, elem_name);
            if (resolved) {
                snprintf(buf, sizeof(buf), "__Vec_%s", type_mangle_name(resolved));
            } else {
                snprintf(buf, sizeof(buf), "__Vec_%s", elem_name);
            }
            return xstrdup(buf);
        } else if (elem->type == NODE_GENERIC_TYPE) {
            char* mangled = build_mangled_name_from_generic_node(gen, elem);
            snprintf(buf, sizeof(buf), "__Vec_%s", mangled);
            free(mangled);
            return xstrdup(buf);
        }
        return NULL;
    }
    // Generic struct type — build mangled name
    char* mangled = build_mangled_name_from_generic_node(gen, field_type);
    return mangled;
}

// Emit a string literal with C escape sequences
static void emit_string_lit(CodeGen* gen, Node* node) {
    int idx = lookup_string_lit(gen, node->as.string_lit.value, node->as.string_lit.length);
    if (idx >= 0) {
        emit(gen, "((const char*)__rc_str_%d.data)", idx);
    } else {
        // Fallback: inline C string literal (should not happen for well-collected AST)
        emit(gen, "\"");
        for (int i = 0; i < node->as.string_lit.length; i++) {
            char c = node->as.string_lit.value[i];
            switch (c) {
            case '\n':
                emit(gen, "\\n");
                break;
            case '\t':
                emit(gen, "\\t");
                break;
            case '\r':
                emit(gen, "\\r");
                break;
            case '\\':
                emit(gen, "\\\\");
                break;
            case '"':
                emit(gen, "\\\"");
                break;
            default:
                emit(gen, "%c", c);
                break;
            }
        }
        emit(gen, "\"");
    }
}

// Emit a character literal with C escape sequences
static void emit_char_lit(CodeGen* gen, Node* node) {
    if (node->as.char_lit.value == '\n') {
        emit(gen, "'\\n'");
    } else if (node->as.char_lit.value == '\t') {
        emit(gen, "'\\t'");
    } else if (node->as.char_lit.value == '\r') {
        emit(gen, "'\\r'");
    } else if (node->as.char_lit.value == '\\') {
        emit(gen, "'\\\\'");
    } else if (node->as.char_lit.value == '\'') {
        emit(gen, "'\\''");
    } else {
        emit(gen, "'%c'", node->as.char_lit.value);
    }
}

// Emit an enum variant: simple tag, bare data enum, or data enum with constructor args
static void emit_enum_value(CodeGen* gen, Node* node) {
    const char* enum_name    = enum_value_resolved_name(gen, node);
    int         enum_len     = enum_value_resolved_name_length(gen, node);
    int         is_data_enum = sem_info_get_enum_value_is_data_enum(gen->checker.sem, node,
                                                                    node->as.enum_value.is_data_enum);

    if (!is_data_enum) {
        // Simple enum: emit qualified value name (EnumName_ValueName)
        emit(gen, "%.*s_%.*s", enum_len, enum_name, node->as.enum_value.value_name_length,
             node->as.enum_value.value_name);
    } else if (node->as.enum_value.args.count == 0) {
        // Data enum, bare tag: (EnumName){.tag = EnumName_ValueName}
        emit(gen, "(%.*s){.tag = %.*s_%.*s}", enum_len, enum_name, enum_len, enum_name,
             node->as.enum_value.value_name_length, node->as.enum_value.value_name);
    } else {
        // Data enum with args: (EnumName){.tag = EnumName_ValueName, .ValueName = {.f0 = ..}}
        int needs_rc_inc = 0;
        for (int i = 0; i < node->as.enum_value.args.count; i++) {
            Node* arg = node->as.enum_value.args.nodes[i];
            if (arg->type == NODE_IDENT && rc_is_tracked(gen, arg->as.ident.name)) {
                needs_rc_inc = 1;
                break;
            }
        }

        if (needs_rc_inc) {
            emit(gen, "({ ");
            for (int i = 0; i < node->as.enum_value.args.count; i++) {
                Node* arg = node->as.enum_value.args.nodes[i];
                if (arg->type == NODE_IDENT && rc_is_tracked(gen, arg->as.ident.name)) {
                    Type*       arg_type = rc_get_var_type(gen, arg->as.ident.name);
                    const char* inc_fn   = get_inc_func_for_type(arg_type);
                    emit(gen, "%s(%s); ", inc_fn, arg->as.ident.name);
                    free((char*)inc_fn);
                }
            }
        }

        emit(gen, "(%.*s){.tag = %.*s_%.*s, .%.*s = {", enum_len, enum_name, enum_len, enum_name,
             node->as.enum_value.value_name_length, node->as.enum_value.value_name,
             node->as.enum_value.value_name_length, node->as.enum_value.value_name);
        for (int i = 0; i < node->as.enum_value.args.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            emit(gen, ".f%d = ", i);
            emit_expr(gen, node->as.enum_value.args.nodes[i]);
        }
        emit(gen, "}}");

        if (needs_rc_inc) {
            emit(gen, "; })");
        }
    }
}

static int call_ident_matches(Node* ident, const char* name) {
    int len = (int)strlen(name);
    return ident && ident->type == NODE_IDENT && ident->as.ident.length == len &&
           strncmp(ident->as.ident.name, name, ident->as.ident.length) == 0;
}

static void emit_call_args(CodeGen* gen, Node* call, int leading_comma) {
    for (int i = 0; i < call->as.call.args.count; i++) {
        if (leading_comma || i > 0) {
            emit(gen, ", ");
        }
        emit_expr(gen, call->as.call.args.nodes[i]);
    }
}

static int is_std_format_member_call(const char* module_name, Node* func) {
    return module_name && strcmp(module_name, "std") == 0 && func->type == NODE_MEMBER &&
           func->as.member.length == 6 &&
           strncmp(func->as.member.name, "format", func->as.member.length) == 0;
}

static void emit_module_member_call(CodeGen* gen, Node* call, Node* func, const char* module_name) {
    emit(gen, "%s_%.*s(", module_name, func->as.member.length, func->as.member.name);
    emit_call_args(gen, call, 0);
    emit(gen, ")");
}

static void emit_method_receiver(CodeGen* gen, Node* func, const char* callee_struct_name) {
    if (!is_enum_type_name(gen, callee_struct_name)) {
        emit_expr(gen, func->as.member.object);
        return;
    }

    if (gen->in_enum_method && call_ident_matches(func->as.member.object, "self")) {
        emit(gen, "self");
    } else if (func->as.member.object->type == NODE_IDENT) {
        emit(gen, "&");
        emit_expr(gen, func->as.member.object);
    } else {
        int tmp = gen->out.temp_count++;
        emit(gen, "({%s __tmp%d = ", callee_struct_name, tmp);
        emit_expr(gen, func->as.member.object);
        emit(gen, "; &__tmp%d;})", tmp);
    }
}

static void emit_struct_method_call(CodeGen* gen, Node* call, Node* func,
                                    const char* callee_struct_name) {
    emit(gen, "%s_%.*s(", callee_struct_name, func->as.member.length, func->as.member.name);
    emit_method_receiver(gen, func, callee_struct_name);
    emit_call_args(gen, call, 1);
    emit(gen, ")");
}

static int is_generic_module_call(CodeGen* gen, Node* func) {
    if (func->as.member.object->type != NODE_IDENT) {
        return 0;
    }

    const char* obj_name = func->as.member.object->as.ident.name;
    for (int i = 0; i < gen->generics.module_count; i++) {
        if (strcmp(gen->generics.modules[i], obj_name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void emit_generic_member_call(CodeGen* gen, Node* call, Node* func) {
    if (is_generic_module_call(gen, func)) {
        emit(gen, "%s_%.*s(", func->as.member.object->as.ident.name, func->as.member.length,
             func->as.member.name);
        emit_call_args(gen, call, 0);
        emit(gen, ")");
        return;
    }

    char* resolved_name = resolve_generic_method_target(gen, func);
    if (resolved_name) {
        emit(gen, "%s_%.*s(", resolved_name, func->as.member.length, func->as.member.name);
        emit_expr(gen, func->as.member.object);
        emit_call_args(gen, call, 1);
        emit(gen, ")");
        free(resolved_name);
        return;
    }

    emit_expr(gen, func);
    emit(gen, "(");
    emit_call_args(gen, call, 0);
    emit(gen, ")");
}

static const char* find_name_alias(NameAlias* aliases, int count, Node* func) {
    if (func->type != NODE_IDENT) {
        return NULL;
    }

    for (int i = 0; i < count; i++) {
        if (strncmp(aliases[i].whist_name, func->as.ident.name, func->as.ident.length) == 0 &&
            aliases[i].whist_name[func->as.ident.length] == '\0') {
            return aliases[i].c_name;
        }
    }
    return NULL;
}

static const char* find_call_alias(CodeGen* gen, Node* func) {
    const char* alias = find_name_alias(gen->aliases.externs, gen->aliases.extern_count, func);
    if (alias) {
        return alias;
    }
    return find_name_alias(gen->aliases.uses, gen->aliases.use_count, func);
}

static int is_extern_call(CodeGen* gen, Node* func) {
    if (func->type != NODE_IDENT) {
        return 0;
    }

    for (int i = 0; i < gen->aliases.extern_func_count; i++) {
        if (strncmp(gen->aliases.extern_funcs[i], func->as.ident.name, func->as.ident.length) ==
                0 &&
            gen->aliases.extern_funcs[i][func->as.ident.length] == '\0') {
            return 1;
        }
    }
    return 0;
}

static void emit_regular_call(CodeGen* gen, Node* call, Node* func) {
    // Generic free function calls use the checker-set mangled name
    if (call->as.call.resolved_name) {
        emit(gen, "%s(", call->as.call.resolved_name);
        emit_call_args(gen, call, 0);
        emit(gen, ")");
        return;
    }

    const char* alias_name = find_call_alias(gen, func);
    if (alias_name) {
        emit(gen, "%s(", alias_name);
        emit_call_args(gen, call, 0);
        emit(gen, ")");
        return;
    }

    if (gen->current_module && func->type == NODE_IDENT && !is_extern_call(gen, func)) {
        emit(gen, "%s_", gen->current_module);
    }

    if (gen->current_module == NULL && call_ident_matches(func, "main")) {
        emit(gen, "__w0_user_main(");
    } else {
        emit_expr(gen, func);
        emit(gen, "(");
    }
    emit_call_args(gen, call, 0);
    emit(gen, ")");
}

// Emit a function call: module-qualified, method, generic method, or regular call
static void emit_call_expr(CodeGen* gen, Node* node) {
    Node* func = node->as.call.func;

    // sameref(a, b) -> (a == b) pointer comparison
    if (func->type == NODE_IDENT && func->as.ident.length == 7 &&
        strncmp(func->as.ident.name, "sameref", 7) == 0) {
        emit(gen, "(");
        emit_expr(gen, node->as.call.args.nodes[0]);
        emit(gen, " == ");
        emit_expr(gen, node->as.call.args.nodes[1]);
        emit(gen, ")");
        return;
    }

    // assert(expr) builtin
    if (func->type == NODE_IDENT && func->as.ident.length == 6 &&
        strncmp(func->as.ident.name, "assert", 6) == 0) {
        Node*       arg      = node->as.call.args.nodes[0];
        char*       expr_str = stringify_expr(arg);
        const char* file     = gen->source_file ? gen->source_file : "<unknown>";
        emit(gen, "do { if (!(");
        emit_expr(gen, arg);
        emit(gen,
             ")) { fprintf(stderr, \"ASSERT FAILED: %%s\\n  at %%s:%%d\\n\", "
             "\"%s\", \"%s\", %d); longjmp(__test_jmp_buf, 1); } } while(0)",
             expr_str, file, node->line);
        return;
    }

    const char* callee_module_name = NULL;
    const char* callee_struct_name = NULL;
    if (func->type == NODE_MEMBER) {
        callee_module_name = member_module_name(gen, func);
        callee_struct_name = member_struct_name(gen, func);
    }

    if (func->type == NODE_MEMBER && callee_module_name != NULL) {
        if (is_std_format_member_call(callee_module_name, func)) {
            emit(gen, "__std_format(");
            emit_call_args(gen, node, 0);
            emit(gen, ")");
        } else {
            emit_module_member_call(gen, node, func, callee_module_name);
        }
        return;
    }

    if (func->type == NODE_MEMBER && callee_struct_name != NULL && !func->as.member.is_method_ref) {
        emit_struct_method_call(gen, node, func, callee_struct_name);
        return;
    }

    if (func->type == NODE_MEMBER && callee_struct_name == NULL && gen->generics.subst) {
        emit_generic_member_call(gen, node, func);
        return;
    }

    emit_regular_call(gen, node, func);
}

// Emit an index expression: bounds-checked vec/span, tuple field, or array access
static void emit_index_expr(CodeGen* gen, Node* node) {
    if (node->as.index.is_vec_index) {
        // Bounds-checked vec access
        emit(gen, "(__w0_vec_check(");
        emit_expr(gen, node->as.index.object);
        emit(gen, "->count, ");
        emit_expr(gen, node->as.index.index);
        emit(gen, ", %d, %d), ", node->line, node->column);
        emit_expr(gen, node->as.index.object);
        emit(gen, "->data[");
        emit_expr(gen, node->as.index.index);
        emit(gen, "])");
    } else if (node->as.index.is_span_index) {
        // Bounds-checked span access
        emit(gen, "(__w0_span_check(");
        emit_expr(gen, node->as.index.object);
        emit(gen, ".count, ");
        emit_expr(gen, node->as.index.index);
        emit(gen, ", %d, %d), ", node->line, node->column);
        emit_expr(gen, node->as.index.object);
        emit(gen, ".data[");
        emit_expr(gen, node->as.index.index);
        emit(gen, "])");
    } else if (node->as.index.is_tuple_index) {
        // Tuple indexing: obj._N
        emit_expr(gen, node->as.index.object);
        emit(gen, "._%ld", node->as.index.index->as.int_lit.value);
    } else {
        // Array/string indexing: obj[index]
        emit_expr(gen, node->as.index.object);
        emit(gen, "[");
        emit_expr(gen, node->as.index.index);
        emit(gen, "]");
    }
}

// Emit a slice expression as a Span compound literal with data pointer and count
static void emit_slice_expr(CodeGen* gen, Node* node) {
    // String slicing: produces a new string via __String_substr
    if (node->as.slice.is_string) {
        emit(gen, "__String_substr(");
        emit_expr(gen, node->as.slice.object);
        emit(gen, ", ");
        if (node->as.slice.start) {
            emit_expr(gen, node->as.slice.start);
        } else {
            emit(gen, "0");
        }
        emit(gen, ", ");
        if (node->as.slice.end) {
            emit_expr(gen, node->as.slice.end);
        } else {
            emit(gen, "(int64_t)strlen(");
            emit_expr(gen, node->as.slice.object);
            emit(gen, ")");
        }
        emit(gen, ")");
        return;
    }

    // Slice produces a span: (__Span_T){ .data = ..., .count = ... }
    Type* span_type = node->as.slice.resolved_type;
    Type* elem_type = span_type->as.span.elem;

    // Emit compound literal
    emit(gen, "((__Span_%s){ .data = ", type_mangle_name(elem_type));

    if (node->as.slice.is_vec) {
        // Vec slicing: .data = vec->data + start
        emit_expr(gen, node->as.slice.object);
        emit(gen, "->data + ");
        if (node->as.slice.start) {
            emit_expr(gen, node->as.slice.start);
        } else {
            emit(gen, "0");
        }
    } else if (node->as.slice.is_array) {
        // Array slicing: .data = &arr[start]
        emit(gen, "&(");
        emit_expr(gen, node->as.slice.object);
        emit(gen, ")[");
        if (node->as.slice.start) {
            emit_expr(gen, node->as.slice.start);
        } else {
            emit(gen, "0");
        }
        emit(gen, "]");
    } else {
        // Span slicing: .data = span.data + start
        emit_expr(gen, node->as.slice.object);
        emit(gen, ".data + ");
        if (node->as.slice.start) {
            emit_expr(gen, node->as.slice.start);
        } else {
            emit(gen, "0");
        }
    }

    emit(gen, ", .count = ");

    // Calculate count: end - start
    // For omitted end, use array length or span/vec.count
    if (node->as.slice.end) {
        emit(gen, "(");
        emit_expr(gen, node->as.slice.end);
        emit(gen, ")");
    } else {
        // Use full length
        if (node->as.slice.is_vec) {
            emit_expr(gen, node->as.slice.object);
            emit(gen, "->count");
        } else if (node->as.slice.is_array) {
            emit(gen, "(sizeof(");
            emit_expr(gen, node->as.slice.object);
            emit(gen, ")/sizeof((");
            emit_expr(gen, node->as.slice.object);
            emit(gen, ")[0]))");
        } else {
            emit_expr(gen, node->as.slice.object);
            emit(gen, ".count");
        }
    }

    // Subtract start if present
    if (node->as.slice.start) {
        emit(gen, " - (");
        emit_expr(gen, node->as.slice.start);
        emit(gen, ")");
    }

    emit(gen, " })");
}

// Emit a member access expression using -> for struct pointers or . for value types
static void emit_member_expr(CodeGen* gen, Node* node) {
    // Unbound method reference: Type.method -> StructName_method
    if (node->as.member.is_method_ref) {
        const char* sname = member_struct_name(gen, node);
        emit(gen, "%s_%.*s", sname, node->as.member.length, node->as.member.name);
        return;
    }

    const char* resolved_struct_name = member_struct_name(gen, node);
    const char* resolved_module_name = member_module_name(gen, node);
    int         resolved_is_ref      = member_is_ref(gen, node);

    // Check if this is module-qualified access (already handled struct_name case)
    if (resolved_struct_name == NULL && resolved_module_name == NULL) {
        // Check if object is 'self' - always a pointer in methods
        // This handles generic methods where is_ref isn't set because body isn't type-checked
        int is_self = (node->as.member.object->type == NODE_IDENT &&
                       strcmp(node->as.member.object->as.ident.name, "self") == 0);

        if (resolved_is_ref || is_self) {
            // Struct reference or self - use ->
            emit_expr(gen, node->as.member.object);
            emit(gen, "->%.*s", node->as.member.length, node->as.member.name);
        } else {
            // Value type member access (tuples, spans) - use .
            emit_expr(gen, node->as.member.object);
            emit(gen, ".%.*s", node->as.member.length, node->as.member.name);
        }
    } else {
        // Struct method or module access
        emit_expr(gen, node->as.member.object);
        emit(gen, "->%.*s", node->as.member.length, node->as.member.name);
    }
}

// Emit a `new` expression as a GCC statement expression: __rc_alloc + field init
static void emit_new_expr(CodeGen* gen, Node* node) {
    Type* rtype = node->as.new_expr.resolved_type;
    // In generic method bodies, the checker doesn't visit the body, so resolved_type
    // may be NULL. Resolve from the type_node using the current substitution context.
    if (!rtype) {
        Node* tn = node->as.new_expr.type_node;
        if (tn->type == NODE_GENERIC_TYPE) {
            if (strcmp(tn->as.generic_type.base_name, "Vec") == 0) {
                // Look up the Vec instance by mangled name
                char* mangled = build_mangled_name_from_generic_node(gen, tn);
                for (int vi = 0; vi < gen->checker.vec_count; vi++) {
                    if (strcmp(gen->checker.vecs[vi].mangled_name, mangled) == 0) {
                        rtype = gen->checker.vecs[vi].type;
                        break;
                    }
                }
                free(mangled);
            } else {
                // Look up the generic struct instance by mangled name
                char* mangled = build_mangled_name_from_generic_node(gen, tn);
                for (int gi = 0; gi < gen->checker.instance_count; gi++) {
                    if (strcmp(gen->checker.instances[gi].mangled_name, mangled) == 0) {
                        rtype = gen->checker.instances[gi].type;
                        break;
                    }
                }
                free(mangled);
            }
        } else if (tn->type == NODE_IDENT) {
            // Simple type name — check substitution context first
            Type* resolved = subst_lookup(gen, tn->as.ident.name);
            if (resolved)
                rtype = resolved;
        }
    }
    if (!rtype) {
        fprintf(stderr,
                "codegen: emit_new_expr: could not resolve type for new expression at line %d\n",
                node->line);
        return;
    }
    if (rtype->kind == TYPE_STRINGBUILDER) {
        // new StringBuilder{} as inline expression using GCC statement expression
        int tmp = gen->out.temp_count++;
        emit(gen,
             "({ __StringBuilder* __rc_tmp%d = (__StringBuilder*)__rc_alloc("
             "sizeof(__StringBuilder), __StringBuilder_cleanup); "
             "__rc_tmp%d->data = NULL; __rc_tmp%d->count = 0; __rc_tmp%d->capacity = 0; "
             "__rc_tmp%d; })",
             tmp, tmp, tmp, tmp, tmp);
        return;
    }
    if (rtype->kind == TYPE_VEC) {
        // new Vec<T>{elems} as inline expression using GCC statement expression
        const char* elem_tname = type_mangle_name(rtype->as.vec.elem);
        int         tmp        = gen->out.temp_count++;
        emit(gen,
             "({ __Vec_%s* __rc_tmp%d = (__Vec_%s*)__rc_alloc(sizeof(__Vec_%s), "
             "__Vec_%s_cleanup); "
             "__rc_tmp%d->data = NULL; __rc_tmp%d->count = 0; __rc_tmp%d->capacity = 0;",
             elem_tname, tmp, elem_tname, elem_tname, elem_tname, tmp, tmp, tmp);
        // Push initial elements
        Node* init = node->as.new_expr.init;
        for (int i = 0; i < init->as.struct_init.fields.count; i++) {
            Node* field = init->as.struct_init.fields.nodes[i];
            if (field && field->type == NODE_FIELD_INIT) {
                emit(gen, " __Vec_%s_push(__rc_tmp%d, ", elem_tname, tmp);
                emit_expr(gen, field->as.field_init.value);
                emit(gen, ");");
            }
        }
        emit(gen, " __rc_tmp%d; })", tmp);
    } else if (node->as.new_expr.init == NULL) {
        // Init-call form: new Type(args) as inline expression
        const char* tname   = rtype->as.struc.name;
        char*       cleanup = get_cleanup_func_for_type(rtype);
        int         tmp     = gen->out.temp_count++;
        emit(gen, "({ %s* __rc_tmp%d = (%s*)__rc_alloc(sizeof(%s), %s); ", tname, tmp, tname, tname,
             cleanup ? cleanup : "NULL");
        emit(gen, "*__rc_tmp%d = (%s){0}; ", tmp, tname);
        emit(gen, "%s_init(__rc_tmp%d", tname, tmp);
        for (int i = 0; i < node->as.new_expr.args.count; i++) {
            emit(gen, ", ");
            emit_expr(gen, node->as.new_expr.args.nodes[i]);
        }
        emit(gen, ");");
        // Increment refcount for any RC-tracked idents passed as args
        for (int i = 0; i < node->as.new_expr.args.count; i++) {
            Node* arg = node->as.new_expr.args.nodes[i];
            if (arg->type == NODE_IDENT && rc_is_tracked(gen, arg->as.ident.name)) {
                Type*       vtype  = rc_get_var_type(gen, arg->as.ident.name);
                const char* inc_fn = get_inc_func_for_type(vtype);
                emit(gen, " %s(%s);", inc_fn, arg->as.ident.name);
                free((char*)inc_fn);
            }
        }
        emit(gen, " __rc_tmp%d; })", tmp);
        free(cleanup);
    } else {
        // new Type { fields } as inline expression using GCC statement expression
        const char* tname   = rtype->as.struc.name;
        char*       cleanup = get_cleanup_func_for_type(rtype);
        int         tmp     = gen->out.temp_count++;
        if (cleanup) {
            emit(gen, "({ %s* __rc_tmp%d = (%s*)__rc_alloc(sizeof(%s), %s); *__rc_tmp%d = (%s)",
                 tname, tmp, tname, tname, cleanup, tmp, tname);
        } else {
            emit(gen, "({ %s* __rc_tmp%d = (%s*)__rc_alloc(sizeof(%s), NULL); *__rc_tmp%d = (%s)",
                 tname, tmp, tname, tname, tmp, tname);
        }
        free(cleanup);
        emit_struct_init(gen, node->as.new_expr.init);
        emit(gen, ";");
        // Increment refcount for any RC-tracked idents stored in struct fields
        Node* rc_init = node->as.new_expr.init;
        for (int i = 0; i < rc_init->as.struct_init.fields.count; i++) {
            Node* field = rc_init->as.struct_init.fields.nodes[i];
            if (field && field->type == NODE_FIELD_INIT &&
                field->as.field_init.value->type == NODE_IDENT &&
                rc_is_tracked(gen, field->as.field_init.value->as.ident.name)) {
                const char* vname  = field->as.field_init.value->as.ident.name;
                Type*       vtype  = rc_get_var_type(gen, vname);
                const char* inc_fn = get_inc_func_for_type(vtype);
                emit(gen, " %s(%s);", inc_fn, vname);
                free((char*)inc_fn);
            }
        }
        emit(gen, " __rc_tmp%d; })", tmp);
    }
}

// Emit string interpolation: $"text {expr} text" -> __std_format("fmt", args...)
static void emit_string_interp(CodeGen* gen, Node* node) {
    int count = node->as.string_interp.part_count;

    // Optimization: if no expression parts, emit as a plain C string literal
    int has_expr = 0;
    for (int i = 0; i < count; i++) {
        if (node->as.string_interp.parts.nodes[i]->type != NODE_STRING_LIT) {
            has_expr = 1;
            break;
        }
    }

    if (!has_expr) {
        // All parts are text — concatenate and look up in string literal table
        int total = 0;
        for (int i = 0; i < count; i++)
            total += node->as.string_interp.parts.nodes[i]->as.string_lit.length;
        char* concat = (char*)malloc(total + 1);
        int   pos    = 0;
        for (int i = 0; i < count; i++) {
            Node* p = node->as.string_interp.parts.nodes[i];
            memcpy(concat + pos, p->as.string_lit.value, p->as.string_lit.length);
            pos += p->as.string_lit.length;
        }
        concat[total] = '\0';
        int idx       = lookup_string_lit(gen, concat, total);
        free(concat);
        if (idx >= 0) {
            emit(gen, "((const char*)__rc_str_%d.data)", idx);
        } else {
            // Fallback: inline C string literal
            emit(gen, "\"");
            for (int i = 0; i < count; i++) {
                Node*       part = node->as.string_interp.parts.nodes[i];
                const char* s    = part->as.string_lit.value;
                int         n    = part->as.string_lit.length;
                for (int j = 0; j < n; j++) {
                    switch (s[j]) {
                    case '\n':
                        emit(gen, "\\n");
                        break;
                    case '\t':
                        emit(gen, "\\t");
                        break;
                    case '\r':
                        emit(gen, "\\r");
                        break;
                    case '\\':
                        emit(gen, "\\\\");
                        break;
                    case '"':
                        emit(gen, "\\\"");
                        break;
                    case '\0':
                        emit(gen, "\\0");
                        break;
                    default:
                        emit(gen, "%c", s[j]);
                        break;
                    }
                }
            }
            emit(gen, "\"");
        }
        return;
    }

    // Build __std_format("fmt", args...)
    emit(gen, "__std_format(\"");

    // First pass: format string
    for (int i = 0; i < count; i++) {
        Node* part = node->as.string_interp.parts.nodes[i];
        Type* t    = node->as.string_interp.part_types[i];
        if (part->type == NODE_STRING_LIT) {
            // Text segment: emit with C escaping, and escape % as %%
            const char* s = part->as.string_lit.value;
            int         n = part->as.string_lit.length;
            for (int j = 0; j < n; j++) {
                switch (s[j]) {
                case '\n':
                    emit(gen, "\\n");
                    break;
                case '\t':
                    emit(gen, "\\t");
                    break;
                case '\r':
                    emit(gen, "\\r");
                    break;
                case '\\':
                    emit(gen, "\\\\");
                    break;
                case '"':
                    emit(gen, "\\\"");
                    break;
                case '\0':
                    emit(gen, "\\0");
                    break;
                case '%':
                    emit(gen, "%%%%");
                    break;
                default:
                    emit(gen, "%c", s[j]);
                    break;
                }
            }
        } else {
            // Expression: emit format specifier based on type
            if (!t)
                continue;
            switch (t->kind) {
            case TYPE_STRING:
                emit(gen, "%%s");
                break;
            case TYPE_BOOL:
                emit(gen, "%%s");
                break;
            case TYPE_CHAR:
                emit(gen, "%%c");
                break;
            case TYPE_F32:
            case TYPE_F64:
                emit(gen, "%%g");
                break;
            case TYPE_INT64:
                emit(gen, "%%lld");
                break;
            case TYPE_UINT64:
                emit(gen, "%%llu");
                break;
            default:
                // Other integer types
                emit(gen, "%%d");
                break;
            }
        }
    }
    emit(gen, "\"");

    // Second pass: arguments
    for (int i = 0; i < count; i++) {
        Node* part = node->as.string_interp.parts.nodes[i];
        Type* t    = node->as.string_interp.part_types[i];
        if (part->type == NODE_STRING_LIT)
            continue;
        if (!t)
            continue;
        emit(gen, ", ");
        if (t->kind == TYPE_BOOL) {
            emit(gen, "(");
            emit_expr(gen, part);
            emit(gen, " ? \"true\" : \"false\")");
        } else {
            emit_expr(gen, part);
        }
    }

    emit(gen, ")");
}

// Emit a try expression (expr?) using GCC statement expression.
// For Result: ({ EnumType __tryN = expr; if (__tryN.tag == Enum_Err) { rc_cleanup; return/goto; }
//               __tryN.Ok.f0; })
// For Option: ({ EnumType __tryN = expr; if (__tryN.tag == Enum_None) { rc_cleanup; return/goto; }
//               __tryN.Some.f0; })
static void emit_try_expr(CodeGen* gen, Node* node) {
    const char* enum_name     = node->as.try_expr.enum_name;
    const char* ret_enum_name = node->as.try_expr.ret_enum_name;
    int         is_option     = node->as.try_expr.is_option;
    int         try_id        = gen->out.temp_count++;

    // Open GCC statement expression
    emit(gen, "({ %s __try%d = ", enum_name, try_id);
    emit_expr(gen, node->as.try_expr.expr);
    emit(gen, "; ");

    // Tag check
    if (is_option) {
        emit(gen, "if (__try%d.tag == %s_None) { ", try_id, enum_name);
    } else {
        emit(gen, "if (__try%d.tag == %s_Err) { ", try_id, enum_name);
    }

    // For Result: save error payload before RC cleanup to avoid use-after-free
    // when the source enum is an RC-tracked variable whose Err payload would be freed
    if (!is_option) {
        emit(gen, "typeof(__try%d.Err.f0) __try_err%d = __try%d.Err.f0; ", try_id, try_id, try_id);
    }

    // Inline RC cleanup (no newlines, stay inside statement expr)
    for (int i = 0; i < gen->rc.count; i++) {
        Type* t = gen->rc.vars[i].type;
        if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
            emit(gen, "__rc_dec_%s(%s); ", t->as.enm.name, gen->rc.vars[i].name);
        } else {
            emit(gen, "__rc_dec(%s); ", gen->rc.vars[i].name);
        }
    }

    // Early return with error/none value
    if (gen->defer.count > 0) {
        // With defers: store in __ret and goto __cleanup
        if (is_option) {
            emit(gen, "__ret = (%s){.tag = %s_None}; ", ret_enum_name, ret_enum_name);
        } else {
            emit(gen, "__ret = (%s){.tag = %s_Err, .Err = {.f0 = __try_err%d}}; ", ret_enum_name,
                 ret_enum_name, try_id);
        }
        emit(gen, "goto __cleanup; ");
    } else {
        // Direct return
        if (is_option) {
            emit(gen, "return (%s){.tag = %s_None}; ", ret_enum_name, ret_enum_name);
        } else {
            emit(gen, "return (%s){.tag = %s_Err, .Err = {.f0 = __try_err%d}}; ", ret_enum_name,
                 ret_enum_name, try_id);
        }
    }

    // Close if, yield unwrapped value
    if (is_option) {
        emit(gen, "} __try%d.Some.f0; })", try_id);
    } else {
        emit(gen, "} __try%d.Ok.f0; })", try_id);
    }
}

// Emit match-as-expression via GCC statement expression:
// ({ Enum __matchN = <expr>; Result __matchvN; if (...) { __matchvN = ...; } ... __matchvN; })
static void emit_match_expr(CodeGen* gen, Node* node) {
    Type* enum_type  = node->as.match_stmt.resolved_type;
    Type* value_type = node->as.match_stmt.resolved_value_type;
    if (!enum_type || enum_type->kind != TYPE_ENUM || !value_type ||
        value_type->kind == TYPE_ERROR) {
        emit(gen, "/* invalid match expr */");
        return;
    }

    int         is_data   = enum_type->as.enm.has_data;
    const char* enum_name = enum_type->as.enm.name;
    int         match_id  = gen->out.temp_count++;

    emit(gen, "({ %s __match%d = ", enum_name, match_id);
    emit_expr(gen, node->as.match_stmt.expr);
    emit(gen, "; ");
    emit_resolved_type(gen, value_type);
    emit(gen, " __matchv%d; ", match_id);

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

        if (arm->as.match_arm.is_wildcard) {
            if (first) {
                emit(gen, "{ ");
            } else {
                emit(gen, "else { ");
            }
        } else if (!has_wildcard && a == last_arm && !first) {
            emit(gen, "else { ");
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
            emit(gen, ") { ");
        }
        first = 0;

        if (!arm->as.match_arm.is_wildcard && is_data && arm->as.match_arm.binding_count > 0) {
            const char* variant     = arm->as.match_arm.variant_name;
            int         variant_idx = -1;
            for (int i = 0; i < enum_type->as.enm.value_count; i++) {
                if (strcmp(enum_type->as.enm.value_names[i], variant) == 0) {
                    variant_idx = i;
                    break;
                }
            }
            if (variant_idx >= 0) {
                for (int j = 0; j < arm->as.match_arm.binding_count; j++) {
                    emit_resolved_type(gen, enum_type->as.enm.variant_types[variant_idx][j]);
                    emit(gen, " %s = __match%d.%s.f%d; ", arm->as.match_arm.bindings[j], match_id,
                         variant, j);
                }
            }
        }

        emit(gen, "__matchv%d = ", match_id);
        emit_expr(gen, arm->as.match_arm.body);
        emit(gen, "; } ");
    }

    emit(gen, "__matchv%d; })", match_id);
}

// Dispatch expression code generation based on node type
void emit_expr(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_INT_LIT:
        emit(gen, "%ldLL", node->as.int_lit.value);
        break;

    case NODE_FLOAT_LIT:
        emit(gen, "%g", node->as.float_lit.value);
        break;

    case NODE_STRING_LIT:
        emit_string_lit(gen, node);
        break;

    case NODE_CHAR_LIT:
        emit_char_lit(gen, node);
        break;

    case NODE_BOOL_LIT:
        emit(gen, "%s", node->as.bool_lit.value ? "true" : "false");
        break;

    case NODE_NULL_LIT:
        emit(gen, "NULL");
        break;

    case NODE_IDENT:
        // In enum methods, self is a pointer but represents a value — dereference it
        if (gen->in_enum_method && node->as.ident.length == 4 &&
            memcmp(node->as.ident.name, "self", 4) == 0) {
            emit(gen, "(*self)");
        } else {
            emit(gen, "%.*s", node->as.ident.length, node->as.ident.name);
        }
        break;

    case NODE_ENUM_VALUE:
        emit_enum_value(gen, node);
        break;

    case NODE_BINARY:
        if (node->as.binary.is_eq_op) {
            const char* tname = node->as.binary.eq_type_name;
            emit(gen, "(");
            if (node->as.binary.op == TOK_BANG_EQ)
                emit(gen, "!");
            if (node->as.binary.is_enum_eq)
                emit(gen, "__%s_eq(", tname);
            else
                emit(gen, "%s_eq(", tname);
            emit_expr(gen, node->as.binary.left);
            emit(gen, ", ");
            emit_expr(gen, node->as.binary.right);
            emit(gen, "))");
        } else if (node->as.binary.is_string_op) {
            TokenType op = node->as.binary.op;
            if (op == TOK_EQ_EQ) {
                emit(gen, "(strcmp(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ") == 0)");
            } else if (op == TOK_BANG_EQ) {
                emit(gen, "(strcmp(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ") != 0)");
            } else if (op == TOK_LT) {
                emit(gen, "(strcmp(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ") < 0)");
            } else if (op == TOK_GT) {
                emit(gen, "(strcmp(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ") > 0)");
            } else if (op == TOK_LT_EQ) {
                emit(gen, "(strcmp(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ") <= 0)");
            } else if (op == TOK_GT_EQ) {
                emit(gen, "(strcmp(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ") >= 0)");
            } else if (op == TOK_PLUS) {
                emit(gen, "__String_concat(");
                emit_expr(gen, node->as.binary.left);
                emit(gen, ", ");
                emit_expr(gen, node->as.binary.right);
                emit(gen, ")");
            }
        } else {
            emit(gen, "(");
            emit_expr(gen, node->as.binary.left);
            emit(gen, " %s ", binary_op_str(node->as.binary.op));
            emit_expr(gen, node->as.binary.right);
            emit(gen, ")");
        }
        break;

    case NODE_UNARY:
        emit(gen, "(%s", unary_op_str(node->as.unary.op));
        emit_expr(gen, node->as.unary.operand);
        emit(gen, ")");
        break;

    case NODE_CALL:
        emit_call_expr(gen, node);
        break;

    case NODE_INDEX:
        emit_index_expr(gen, node);
        break;

    case NODE_SLICE:
        emit_slice_expr(gen, node);
        break;

    case NODE_MEMBER:
        emit_member_expr(gen, node);
        break;

    case NODE_ASSIGN:
        emit(gen, "(");
        emit_expr(gen, node->as.assign.target);
        emit(gen, " %s ", assign_op_str(node->as.assign.op));
        emit_expr(gen, node->as.assign.value);
        emit(gen, ")");
        break;

    case NODE_STRUCT_INIT:
        emit_struct_init(gen, node);
        break;

    case NODE_TUPLE_LIT:
        // Tuple literal: (e1, e2, ...) -> {e1, e2, ...}
        emit(gen, "{");
        for (int i = 0; i < node->as.tuple_lit.elements.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            emit_expr(gen, node->as.tuple_lit.elements.nodes[i]);
        }
        emit(gen, "}");
        break;

    case NODE_ARRAY_LIT:
        // Array literal: [e1, e2, ...] -> {e1, e2, ...}
        emit(gen, "{");
        for (int i = 0; i < node->as.array_lit.elements.count; i++) {
            if (i > 0)
                emit(gen, ", ");
            emit_expr(gen, node->as.array_lit.elements.nodes[i]);
        }
        emit(gen, "}");
        break;

    case NODE_NEW_EXPR:
        emit_new_expr(gen, node);
        break;

    case NODE_STRING_INTERP:
        emit_string_interp(gen, node);
        break;

    case NODE_CAST:
        emit(gen, "((");
        emit_resolved_type(gen, node->as.cast_expr.resolved_type);
        emit(gen, ")(");
        emit_expr(gen, node->as.cast_expr.expr);
        emit(gen, "))");
        break;

    case NODE_TRY_EXPR:
        emit_try_expr(gen, node);
        break;

    case NODE_MATCH:
        emit_match_expr(gen, node);
        break;

    default:
        emit(gen, "/* unknown expr %d */", node->type);
        break;
    }
}

// Emit a struct initializer as a C designated initializer: {.field = value, ...}
void emit_struct_init(CodeGen* gen, Node* node) {
    emit(gen, "{");
    for (int i = 0; i < node->as.struct_init.fields.count; i++) {
        Node* field = node->as.struct_init.fields.nodes[i];
        if (!field || field->type != NODE_FIELD_INIT) {
            continue;
        }
        if (i > 0) {
            emit(gen, ", ");
        }
        emit(gen, ".%s = ", field->as.field_init.name);
        emit_expr(gen, field->as.field_init.value);
    }
    emit(gen, "}");
}

// Emit code to extract values from a tuple into variables (recursive for nested patterns)
// temp_prefix is the expression to access the current tuple (e.g., "__tuple0" or "__tuple0._1")
void emit_destruct_pattern(CodeGen* gen, DestructPattern* pattern, const char* temp_prefix,
                           int is_const) {
    if (!pattern)
        return;

    Type* type = pattern->resolved_type;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        // Emit: [const] Type name = temp_prefix;
        emit_indent(gen);
        if (is_const) {
            emit(gen, "const ");
        }
        emit_resolved_type(gen, type);
        emit(gen, " %s = %s;\n", pattern->as.ident.name, temp_prefix);
        break;

    case PATTERN_TUPLE:
        // For tuple patterns, we need to access each element
        // The tuple value is at temp_prefix, elements are temp_prefix._0, temp_prefix._1, etc.
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            DestructPattern* elem = pattern->as.tuple.elements[i];

            // Build the accessor string for this element
            char accessor[256];
            snprintf(accessor, sizeof(accessor), "%s._%d", temp_prefix, i);

            if (elem->kind == PATTERN_TUPLE) {
                // For nested tuple patterns, first create a temp variable for this level
                Type* elem_type = elem->resolved_type;
                emit_indent(gen);
                emit_resolved_type(gen, elem_type);
                int temp_id = gen->out.temp_count++;
                emit(gen, " __tuple%d = %s;\n", temp_id, accessor);

                // Then recursively emit the nested pattern
                char nested_prefix[64];
                snprintf(nested_prefix, sizeof(nested_prefix), "__tuple%d", temp_id);
                emit_destruct_pattern(gen, elem, nested_prefix, is_const);
            } else {
                // Simple identifier - directly assign from accessor
                emit_destruct_pattern(gen, elem, accessor, is_const);
            }
        }
        break;

    case PATTERN_STRUCT:
        // Struct patterns: extract fields via pointer access (->)
        for (int i = 0; i < pattern->as.struc.count; i++) {
            emit_indent(gen);
            if (is_const) {
                emit(gen, "const ");
            }
            emit_resolved_type(gen, pattern->as.struc.field_types[i]);
            emit(gen, " %s = %s->%s;\n", pattern->as.struc.field_names[i], temp_prefix,
                 pattern->as.struc.field_names[i]);
        }
        break;
    }
}
