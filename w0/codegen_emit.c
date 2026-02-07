#include "codegen_emit.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "types.h"
#include "vec.h"

void emit_indent(CodeGen* gen) {
    for (int i = 0; i < gen->indent; i++) {
        fprintf(gen->out, "    ");
    }
}

void emit(CodeGen* gen, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out, fmt, args);
    va_end(args);
}

static void emit_expr(CodeGen* gen, Node* node);
static void emit_struct_init(CodeGen* gen, Node* node);
static void emit_block_contents(CodeGen* gen, Node* block);
static void emit_destruct_pattern(CodeGen* gen, DestructPattern* pattern, const char* temp_prefix,
                                  int is_const);

static void defer_push(CodeGen* gen, Node* node) {
    VEC_GROW(gen->defer_stack, gen->defer_count, gen->defer_capacity);
    gen->defer_stack[gen->defer_count++] = node;
}

void defer_clear(CodeGen* gen) {
    gen->defer_count = 0;
}

// RC variable tracking helpers
static const char* get_dec_func_for_type(Type* t) {
    if (t && t->kind == TYPE_STRUCT && (t->as.struc.has_drop || t->as.struc.has_rc_fields)) {
        // Build "__rc_dec_TypeName"
        size_t len = strlen("__rc_dec_") + strlen(t->as.struc.name) + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__rc_dec_%s", t->as.struc.name);
        return buf;
    }
    if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
        size_t len = strlen("__rc_dec_") + strlen(t->as.enm.name) + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__rc_dec_%s", t->as.enm.name);
        return buf;
    }
    if (t && t->kind == TYPE_VEC) {
        const char* elem_tname = type_name(t->as.vec.elem);
        size_t      len        = strlen("__rc_dec_Vec_") + strlen(elem_tname) + 1;
        char*       buf        = xmalloc(len);
        snprintf(buf, len, "__rc_dec_Vec_%s", elem_tname);
        return buf;
    }
    return xstrdup("__rc_dec");
}

static const char* get_inc_func_for_type(Type* t) {
    if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
        size_t len = strlen("__rc_inc_") + strlen(t->as.enm.name) + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__rc_inc_%s", t->as.enm.name);
        return buf;
    }
    return xstrdup("__rc_inc");
}

static void rc_push_var(CodeGen* gen, const char* name, const char* dec_func, Type* type) {
    VEC_GROW(gen->rc_vars, gen->rc_var_count, gen->rc_var_capacity);
    gen->rc_vars[gen->rc_var_count].name        = xstrdup(name);
    gen->rc_vars[gen->rc_var_count].dec_func    = xstrdup(dec_func);
    gen->rc_vars[gen->rc_var_count].type        = type;
    gen->rc_vars[gen->rc_var_count].scope_depth = gen->rc_scope_depth;
    gen->rc_var_count++;
}

// Emit dec for vars at the given depth, remove them from list
static void rc_cleanup_scope(CodeGen* gen, int depth) {
    int dst = 0;
    for (int i = 0; i < gen->rc_var_count; i++) {
        if (gen->rc_vars[i].scope_depth == depth) {
            emit_indent(gen);
            emit(gen, "%s(%s);\n", gen->rc_vars[i].dec_func, gen->rc_vars[i].name);
            free(gen->rc_vars[i].name);
            free(gen->rc_vars[i].dec_func);
        } else {
            gen->rc_vars[dst++] = gen->rc_vars[i];
        }
    }
    gen->rc_var_count = dst;
}

// Emit dec for ALL remaining vars (skip one by name). Does NOT modify list.
static void rc_cleanup_all(CodeGen* gen, const char* skip_name) {
    for (int i = 0; i < gen->rc_var_count; i++) {
        if (skip_name && strcmp(gen->rc_vars[i].name, skip_name) == 0) {
            continue;
        }
        emit_indent(gen);
        emit(gen, "%s(%s);\n", gen->rc_vars[i].dec_func, gen->rc_vars[i].name);
    }
}

// Free and reset the RC var list (at function boundary)
void rc_clear_all(CodeGen* gen) {
    for (int i = 0; i < gen->rc_var_count; i++) {
        free(gen->rc_vars[i].name);
        free(gen->rc_vars[i].dec_func);
    }
    gen->rc_var_count   = 0;
    gen->rc_scope_depth = 0;
}

// Look up the stored dec_func for a tracked RC variable
static const char* rc_get_dec_func(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->rc_var_count; i++) {
        if (strcmp(gen->rc_vars[i].name, name) == 0)
            return gen->rc_vars[i].dec_func;
    }
    return "__rc_dec";
}

// Look up the stored Type* for a tracked RC variable
static Type* rc_get_var_type(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->rc_var_count; i++) {
        if (strcmp(gen->rc_vars[i].name, name) == 0)
            return gen->rc_vars[i].type;
    }
    return NULL;
}

// Check if a variable name is in the RC tracking list
static int rc_is_tracked(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->rc_var_count; i++) {
        if (strcmp(gen->rc_vars[i].name, name) == 0)
            return 1;
    }
    return 0;
}

// Check if a name is a registered enum type
int is_enum_type_name(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enum_name_count; i++) {
        if (strcmp(gen->enum_names[i], name) == 0)
            return 1;
    }
    return 0;
}

int enum_index(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enum_name_count; i++) {
        if (strcmp(gen->enum_names[i], name) == 0)
            return i;
    }
    return -1;
}

int enum_has_rc_fields(CodeGen* gen, const char* name) {
    int idx = enum_index(gen, name);
    if (idx < 0 || !gen->enum_has_rc_fields)
        return 0;
    return gen->enum_has_rc_fields[idx];
}

// Resolve a type node through aliases. If the node is a NODE_IDENT
// that names a type alias, return the alias target node instead.
static Node* resolve_alias(CodeGen* gen, Node* type_node) {
    if (!type_node || type_node->type != NODE_IDENT)
        return type_node;
    for (int i = 0; i < gen->alias_count; i++) {
        if (strcmp(gen->alias_names[i], type_node->as.ident.name) == 0) {
            return gen->alias_targets[i];
        }
    }
    return type_node;
}

// Check if a type node represents a struct (user-defined) type
static int is_struct_type(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return 0;
    type_node = resolve_alias(gen, type_node);
    // Generic types (Box<i64>) are always struct types, except Span<T>, Vec<T>, and generic enums
    if (type_node->type == NODE_GENERIC_TYPE) {
        if (strcmp(type_node->as.generic_type.base_name, "Span") == 0) {
            return 0;
        }
        if (strcmp(type_node->as.generic_type.base_name, "Vec") == 0) {
            return 0;
        }
        // Check if the mangled name is a registered enum
        char* mangled = build_mangled_name_from_generic_node(gen, type_node);
        int   is_enum = is_enum_type_name(gen, mangled);
        free(mangled);
        if (is_enum)
            return 0;
        return 1;
    }
    if (type_node->type != NODE_IDENT)
        return 0;
    if (is_enum_type_name(gen, type_node->as.ident.name))
        return 0;
    return !type_is_builtin_name(type_node->as.ident.name);
}

int type_node_has_rc(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return 0;
    type_node = resolve_alias(gen, type_node);
    if (type_node->type == NODE_GENERIC_TYPE) {
        if (strcmp(type_node->as.generic_type.base_name, "Span") == 0)
            return 0;
        if (strcmp(type_node->as.generic_type.base_name, "Vec") == 0)
            return 1; // Vec is always RC-managed
        // Check if this is a generic enum
        char* mangled = build_mangled_name_from_generic_node(gen, type_node);
        if (is_enum_type_name(gen, mangled)) {
            int has_rc = enum_has_rc_fields(gen, mangled);
            free(mangled);
            return has_rc;
        }
        free(mangled);
        return 1; // Generic struct — always RC
    }
    if (type_node->type != NODE_IDENT)
        return 0;

    const char* name = type_node->as.ident.name;

    // Handle type parameter substitution
    if (gen->subst_ctx) {
        for (int i = 0; i < gen->subst_ctx->count; i++) {
            if (strcmp(gen->subst_ctx->type_params[i], name) == 0) {
                Type* resolved = gen->subst_ctx->type_args[i];
                if (resolved->kind == TYPE_ENUM)
                    return resolved->as.enm.has_rc_fields;
                if (resolved->kind == TYPE_STRUCT)
                    return 1; // Structs are always RC (heap-allocated pointers)
                return 0;     // Primitives, etc.
            }
        }
    }

    if (type_is_builtin_name(name))
        return 0;
    if (is_enum_type_name(gen, name))
        return enum_has_rc_fields(gen, name);
    return 1;
}

// Resolve a type node to its enum name under substitution, or NULL if not an enum.
// For NODE_GENERIC_TYPE: builds mangled name and checks. For NODE_IDENT with subst_ctx: resolves.
// Returns a static/interned name (caller must NOT free).
const char* resolve_enum_name(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return NULL;
    type_node = resolve_alias(gen, type_node);
    if (type_node->type == NODE_GENERIC_TYPE) {
        char* mangled = build_mangled_name_from_generic_node(gen, type_node);
        if (is_enum_type_name(gen, mangled)) {
            // Return the registered name from enum_names (don't free mangled yet)
            for (int i = 0; i < gen->enum_name_count; i++) {
                if (strcmp(gen->enum_names[i], mangled) == 0) {
                    free(mangled);
                    return gen->enum_names[i];
                }
            }
        }
        free(mangled);
        return NULL;
    }
    if (type_node->type == NODE_IDENT) {
        const char* name = type_node->as.ident.name;
        // Check substitution context
        if (gen->subst_ctx) {
            for (int i = 0; i < gen->subst_ctx->count; i++) {
                if (strcmp(gen->subst_ctx->type_params[i], name) == 0) {
                    Type* resolved = gen->subst_ctx->type_args[i];
                    if (resolved->kind == TYPE_ENUM)
                        return resolved->as.enm.name;
                    return NULL;
                }
            }
        }
        if (is_enum_type_name(gen, name))
            return name;
    }
    return NULL;
}

// Helper to check if a name is a type variable (not a builtin type)
int codegen_is_type_variable(const char* name) {
    return !type_is_builtin_name(name);
}

// Extract additional type bindings from matching a method's receiver pattern against concrete args
// For example, matching <i32, Box<T>> against [i32, Box_i64] extracts T=i64
// Returns 1 on match (and fills out_* params), 0 on no match
int codegen_extract_method_bindings(NodeList* pattern_args, Type** concrete_args, int arg_count,
                                    char*** out_params, Type*** out_args, int* out_count) {
    int    capacity = 4;
    int    count    = 0;
    char** params   = xmalloc(capacity * sizeof(char*));
    Type** args     = xmalloc(capacity * sizeof(Type*));

    for (int i = 0; i < arg_count; i++) {
        Node* pattern  = pattern_args->nodes[i];
        Type* concrete = concrete_args[i];

        if (pattern->type == NODE_IDENT) {
            const char* name = pattern->as.ident.name;
            // If it's a type variable, bind it
            if (codegen_is_type_variable(name)) {
                if (count >= capacity) {
                    capacity *= 2;
                    params = xrealloc(params, capacity * sizeof(char*));
                    args   = xrealloc(args, capacity * sizeof(Type*));
                }
                params[count] = xstrdup(name);
                args[count]   = concrete;
                count++;
            }
            // If it's a concrete type, check it matches (for partial specialization)
            // For now, we trust that the checker validated this
        } else if (pattern->type == NODE_GENERIC_TYPE) {
            // Pattern is like Box<T>, concrete is like Box_i64
            if (concrete->kind != TYPE_STRUCT)
                continue;

            const char* pattern_base  = pattern->as.generic_type.base_name;
            const char* concrete_name = concrete->as.struc.name;

            // Check the base matches
            size_t base_len = strlen(pattern_base);
            if (strncmp(concrete_name, pattern_base, base_len) != 0 ||
                concrete_name[base_len] != '_') {
                free(params);
                free(args);
                return 0;
            }

            // For single type arg, extract the binding
            if (pattern->as.generic_type.type_args.count == 1) {
                Node* nested = pattern->as.generic_type.type_args.nodes[0];
                if (nested->type == NODE_IDENT && codegen_is_type_variable(nested->as.ident.name)) {
                    // Extract type from mangled name suffix
                    const char* suffix    = concrete_name + base_len + 1;
                    Type*       extracted = type_builtin_from_name(suffix);
                    if (!extracted) {
                        // Assume it's a struct type
                        extracted = type_struct(suffix);
                    }

                    if (count >= capacity) {
                        capacity *= 2;
                        params = xrealloc(params, capacity * sizeof(char*));
                        args   = xrealloc(args, capacity * sizeof(Type*));
                    }
                    params[count] = xstrdup(nested->as.ident.name);
                    args[count]   = extracted;
                    count++;
                }
            }
        }
    }

    *out_params = params;
    *out_args   = args;
    *out_count  = count;
    return 1;
}

// Helper to emit function name with appropriate prefix (method or module)
// For methods: emits "StructName_funcname("
// For library functions: emits "module_funcname("
// For main module functions: emits "funcname("
void emit_function_name(CodeGen* gen, const char* func_name, const char* receiver_type,
                        const char* module_name) {
    if (receiver_type != NULL) {
        // Method: prefix with struct name
        emit(gen, " %s_%s(", receiver_type, func_name);
    } else if (module_name != NULL) {
        // Library function: prefix with module name
        emit(gen, " %s_%s(", module_name, func_name);
    } else {
        // Main module function: no prefix
        emit(gen, " %s(", func_name);
    }
}

// Emit a type from a type annotation node
void emit_type(CodeGen* gen, Node* type_node) {
    if (!type_node) {
        emit(gen, "void");
        return;
    }

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;

        // Check for type parameter substitution (for generic methods)
        if (gen->subst_ctx) {
            for (int i = 0; i < gen->subst_ctx->count; i++) {
                if (strcmp(gen->subst_ctx->type_params[i], name) == 0) {
                    // Substitute with concrete type
                    emit_resolved_type(gen, gen->subst_ctx->type_args[i]);
                    return;
                }
            }
        }

        const char* c_type = type_c_name(name);
        if (c_type) {
            emit(gen, "%s", c_type);
        } else if (is_enum_type_name(gen, name)) {
            // Enum type - value type, no pointer
            emit(gen, "%s", name);
        } else {
            // Check if this is a type alias — resolve to target type
            int alias_found = 0;
            for (int i = 0; i < gen->alias_count; i++) {
                if (strcmp(gen->alias_names[i], name) == 0) {
                    emit_type(gen, gen->alias_targets[i]);
                    alias_found = 1;
                    break;
                }
            }
            if (!alias_found) {
                // User-defined struct type - emit as pointer (struct references)
                emit(gen, "%s*", name);
            }
        }
        break;
    }
    case NODE_UNARY:
        // Pointer types no longer supported in the language
        emit(gen, "/* pointer types not supported */");
        break;
    case NODE_ARRAY_TYPE:
        // Array type: [n]T -> T[n] or T*
        emit_type(gen, type_node->as.array_type.elem_type);
        if (type_node->as.array_type.size) {
            emit(gen, "[");
            emit_expr(gen, type_node->as.array_type.size);
            emit(gen, "]");
        } else {
            emit(gen, "*");
        }
        break;
    case NODE_TUPLE_TYPE: {
        // Build a Type* and find the matching typedef
        Type* tuple = type_from_node(type_node);
        int   idx   = -1;
        for (int i = 0; i < gen->tuple_type_count; i++) {
            if (tuple_types_equal(gen->tuple_types[i], tuple)) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            emit(gen, "__tuple_t%d", idx);
        } else {
            // Fallback to inline struct
            emit(gen, "struct { ");
            for (int i = 0; i < type_node->as.tuple_type.elem_types.count; i++) {
                emit_type(gen, type_node->as.tuple_type.elem_types.nodes[i]);
                emit(gen, " _%d; ", i);
            }
            emit(gen, "}");
        }
        break;
    }
    case NODE_GENERIC_TYPE: {
        // Generic type instantiation - emit the mangled name
        // Build the mangled name: Box<i64> -> Box_i64, Box<T> -> Box_i64 (with subst)
        const char* base = type_node->as.generic_type.base_name;

        // Special case: Span<T> is a value type, not a pointer
        int is_span = (strcmp(base, "Span") == 0);
        int is_vec  = (strcmp(base, "Vec") == 0);

        if (is_vec) {
            emit(gen, "__Vec_");
            Node* arg = type_node->as.generic_type.type_args.nodes[0];
            if (arg->type == NODE_IDENT) {
                const char* arg_name    = arg->as.ident.name;
                int         substituted = 0;
                if (gen->subst_ctx) {
                    for (int s = 0; s < gen->subst_ctx->count; s++) {
                        if (strcmp(gen->subst_ctx->type_params[s], arg_name) == 0) {
                            Type* subst_type = gen->subst_ctx->type_args[s];
                            emit(gen, "%s", type_name(subst_type));
                            substituted = 1;
                            break;
                        }
                    }
                }
                if (!substituted) {
                    emit(gen, "%s", arg_name);
                }
            }
            emit(gen, "*"); // Vec is RC-managed pointer
            break;
        }

        if (is_span) {
            emit(gen, "__Span_");
            // Emit the element type name
            Node* arg = type_node->as.generic_type.type_args.nodes[0];
            if (arg->type == NODE_IDENT) {
                const char* arg_name    = arg->as.ident.name;
                int         substituted = 0;
                if (gen->subst_ctx) {
                    for (int s = 0; s < gen->subst_ctx->count; s++) {
                        if (strcmp(gen->subst_ctx->type_params[s], arg_name) == 0) {
                            Type* subst_type = gen->subst_ctx->type_args[s];
                            emit(gen, "%s", type_name(subst_type));
                            substituted = 1;
                            break;
                        }
                    }
                }
                if (!substituted) {
                    emit(gen, "%s", arg_name);
                }
            }
            // No * for spans - they are value types
            break;
        }

        // Regular generic struct (Box, Pair, etc.) - emit as pointer
        emit(gen, "%s", base);
        for (int i = 0; i < type_node->as.generic_type.type_args.count; i++) {
            emit(gen, "_");
            // Get simple type name for mangling
            Node* arg = type_node->as.generic_type.type_args.nodes[i];
            if (arg->type == NODE_IDENT) {
                const char* arg_name = arg->as.ident.name;
                // Check for type parameter substitution
                int substituted = 0;
                if (gen->subst_ctx) {
                    for (int s = 0; s < gen->subst_ctx->count; s++) {
                        if (strcmp(gen->subst_ctx->type_params[s], arg_name) == 0) {
                            // Emit the substituted type's name for mangling
                            Type*       subst_type  = gen->subst_ctx->type_args[s];
                            const char* mangle_name = type_name(subst_type);
                            emit(gen, "%s", mangle_name);
                            substituted = 1;
                            break;
                        }
                    }
                }
                if (!substituted) {
                    emit(gen, "%s", arg_name);
                }
            } else if (arg->type == NODE_GENERIC_TYPE) {
                // Nested generic - recurse to get mangled name
                emit(gen, "%s", arg->as.generic_type.base_name);
                for (int j = 0; j < arg->as.generic_type.type_args.count; j++) {
                    emit(gen, "_");
                    Node* nested = arg->as.generic_type.type_args.nodes[j];
                    if (nested->type == NODE_IDENT) {
                        const char* nested_name = nested->as.ident.name;
                        // Check for type parameter substitution
                        int subst = 0;
                        if (gen->subst_ctx) {
                            for (int s = 0; s < gen->subst_ctx->count; s++) {
                                if (strcmp(gen->subst_ctx->type_params[s], nested_name) == 0) {
                                    Type* subst_type = gen->subst_ctx->type_args[s];
                                    emit(gen, "%s", type_name(subst_type));
                                    subst = 1;
                                    break;
                                }
                            }
                        }
                        if (!subst) {
                            emit(gen, "%s", nested_name);
                        }
                    }
                }
            }
        }
        // Only emit * for struct types, not for generic enums
        {
            char* mangled = build_mangled_name_from_generic_node(gen, type_node);
            if (!is_enum_type_name(gen, mangled)) {
                emit(gen, "*"); // Struct reference
            }
            free(mangled);
        }
        break;
    }
    default:
        emit(gen, "/* unknown type */");
        break;
    }
}

// Emit a resolved Type* (used for inferred types like tuples)
void emit_resolved_type(CodeGen* gen, Type* type) {
    if (!type) {
        emit(gen, "void");
        return;
    }

    switch (type->kind) {
    case TYPE_VOID:
        emit(gen, "void");
        break;
    case TYPE_BOOL:
        emit(gen, "bool");
        break;
    case TYPE_INT64:
        emit(gen, "int64_t");
        break;
    case TYPE_INT8:
        emit(gen, "int8_t");
        break;
    case TYPE_INT16:
        emit(gen, "int16_t");
        break;
    case TYPE_INT32:
        emit(gen, "int32_t");
        break;
    case TYPE_UINT64:
        emit(gen, "uint64_t");
        break;
    case TYPE_UINT8:
        emit(gen, "uint8_t");
        break;
    case TYPE_UINT16:
        emit(gen, "uint16_t");
        break;
    case TYPE_UINT32:
        emit(gen, "uint32_t");
        break;
    case TYPE_F32:
        emit(gen, "float");
        break;
    case TYPE_F64:
        emit(gen, "double");
        break;
    case TYPE_CHAR:
        emit(gen, "char");
        break;
    case TYPE_STRING:
        emit(gen, "const char*");
        break;
    case TYPE_VOIDPTR:
        emit(gen, "void*");
        break;
    case TYPE_ARRAY:
        emit_resolved_type(gen, type->as.array.elem);
        if (type->as.array.size >= 0) {
            emit(gen, "[%d]", type->as.array.size);
        } else {
            emit(gen, "*");
        }
        break;
    case TYPE_STRUCT:
        emit(gen, "%s*", type->as.struc.name);
        break;
    case TYPE_SPAN:
        emit(gen, "__Span_%s", type_name(type->as.span.elem));
        break;
    case TYPE_VEC:
        emit(gen, "__Vec_%s*", type_name(type->as.vec.elem));
        break;
    case TYPE_ENUM:
        emit(gen, "%s", type->as.enm.name);
        break;
    case TYPE_TUPLE: {
        // Find the typedef index for this tuple type
        int idx = -1;
        for (int i = 0; i < gen->tuple_type_count; i++) {
            if (tuple_types_equal(gen->tuple_types[i], type)) {
                idx = i;
                break;
            }
        }
        if (idx >= 0) {
            emit(gen, "__tuple_t%d", idx);
        } else {
            // Fallback to inline struct (shouldn't happen if collection is correct)
            emit(gen, "struct { ");
            for (int i = 0; i < type->as.tuple.elem_count; i++) {
                emit_resolved_type(gen, type->as.tuple.elem_types[i]);
                emit(gen, " _%d; ", i);
            }
            emit(gen, "}");
        }
        break;
    }
    default:
        emit(gen, "/* unknown type */");
        break;
    }
}

// Emit type with variable name (handles array syntax)
void emit_type_with_name(CodeGen* gen, Node* type_node, const char* name) {
    if (!type_node) {
        emit(gen, "void %s", name);
        return;
    }

    if (type_node->type == NODE_ARRAY_TYPE && type_node->as.array_type.size) {
        // Array: T name[n]
        emit_type(gen, type_node->as.array_type.elem_type);
        emit(gen, " %s[", name);
        emit_expr(gen, type_node->as.array_type.size);
        emit(gen, "]");
    } else {
        emit_type(gen, type_node);
        emit(gen, " %s", name);
    }
}

static const char* binary_op_str(TokenType op) {
    switch (op) {
    case TOK_PLUS:
        return "+";
    case TOK_MINUS:
        return "-";
    case TOK_STAR:
        return "*";
    case TOK_SLASH:
        return "/";
    case TOK_PERCENT:
        return "%";
    case TOK_AMP:
        return "&";
    case TOK_PIPE:
        return "|";
    case TOK_CARET:
        return "^";
    case TOK_LT_LT:
        return "<<";
    case TOK_GT_GT:
        return ">>";
    case TOK_EQ_EQ:
        return "==";
    case TOK_BANG_EQ:
        return "!=";
    case TOK_LT:
        return "<";
    case TOK_GT:
        return ">";
    case TOK_LT_EQ:
        return "<=";
    case TOK_GT_EQ:
        return ">=";
    case TOK_AMP_AMP:
        return "&&";
    case TOK_PIPE_PIPE:
        return "||";
    default:
        return "?";
    }
}

static const char* unary_op_str(TokenType op) {
    switch (op) {
    case TOK_MINUS:
        return "-";
    case TOK_BANG:
        return "!";
    case TOK_TILDE:
        return "~";
    case TOK_AMP:
        return "&";
    case TOK_STAR:
        return "*";
    default:
        return "?";
    }
}

static const char* assign_op_str(TokenType op) {
    switch (op) {
    case TOK_EQ:
        return "=";
    case TOK_PLUS_EQ:
        return "+=";
    case TOK_MINUS_EQ:
        return "-=";
    case TOK_STAR_EQ:
        return "*=";
    case TOK_SLASH_EQ:
        return "/=";
    case TOK_PERCENT_EQ:
        return "%=";
    case TOK_AMP_EQ:
        return "&=";
    case TOK_PIPE_EQ:
        return "|=";
    case TOK_CARET_EQ:
        return "^=";
    case TOK_LT_LT_EQ:
        return "<<=";
    case TOK_GT_GT_EQ:
        return ">>=";
    default:
        return "=";
    }
}

static void emit_expr(CodeGen* gen, Node* node) {
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
        emit(gen, "\"");
        // Escape special characters
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
        break;

    case NODE_CHAR_LIT:
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
        break;

    case NODE_BOOL_LIT:
        emit(gen, "%s", node->as.bool_lit.value ? "true" : "false");
        break;

    case NODE_NULL_LIT:
        emit(gen, "NULL");
        break;

    case NODE_IDENT:
        emit(gen, "%.*s", node->as.ident.length, node->as.ident.name);
        break;

    case NODE_ENUM_VALUE:
        if (!node->as.enum_value.is_data_enum) {
            // Simple enum: emit qualified value name (EnumName_ValueName)
            emit(gen, "%.*s_%.*s", node->as.enum_value.enum_name_length,
                 node->as.enum_value.enum_name, node->as.enum_value.value_name_length,
                 node->as.enum_value.value_name);
        } else if (node->as.enum_value.args.count == 0) {
            // Data enum, bare tag: (EnumName){.tag = EnumName_ValueName}
            emit(gen, "(%.*s){.tag = %.*s_%.*s}", node->as.enum_value.enum_name_length,
                 node->as.enum_value.enum_name, node->as.enum_value.enum_name_length,
                 node->as.enum_value.enum_name, node->as.enum_value.value_name_length,
                 node->as.enum_value.value_name);
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

            emit(gen, "(%.*s){.tag = %.*s_%.*s, .%.*s = {", node->as.enum_value.enum_name_length,
                 node->as.enum_value.enum_name, node->as.enum_value.enum_name_length,
                 node->as.enum_value.enum_name, node->as.enum_value.value_name_length,
                 node->as.enum_value.value_name, node->as.enum_value.value_name_length,
                 node->as.enum_value.value_name);
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
        break;

    case NODE_BINARY:
        emit(gen, "(");
        emit_expr(gen, node->as.binary.left);
        emit(gen, " %s ", binary_op_str(node->as.binary.op));
        emit_expr(gen, node->as.binary.right);
        emit(gen, ")");
        break;

    case NODE_UNARY:
        emit(gen, "(%s", unary_op_str(node->as.unary.op));
        emit_expr(gen, node->as.unary.operand);
        emit(gen, ")");
        break;

    case NODE_CALL: {
        Node* func = node->as.call.func;
        // Check if this is a module-qualified call (e.g., std.print())
        if (func->type == NODE_MEMBER && func->as.member.module_name != NULL) {
            // Module-qualified call: emit module_func(args...)
            emit(gen, "%s_%.*s(", func->as.member.module_name, func->as.member.length,
                 func->as.member.name);
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                emit_expr(gen, node->as.call.args.nodes[i]);
            }
            emit(gen, ")");
        } else if (func->type == NODE_MEMBER && func->as.member.struct_name != NULL) {
            // Method call: emit StructName_method(obj, args...)
            // With struct references, objects are already pointers
            emit(gen, "%s_%.*s(", func->as.member.struct_name, func->as.member.length,
                 func->as.member.name);
            // Emit the receiver as first argument (already a pointer)
            emit_expr(gen, func->as.member.object);
            // Emit remaining arguments
            for (int i = 0; i < node->as.call.args.count; i++) {
                emit(gen, ", ");
                emit_expr(gen, node->as.call.args.nodes[i]);
            }
            emit(gen, ")");
        } else {
            // Regular function call
            emit_expr(gen, func);
            emit(gen, "(");
            for (int i = 0; i < node->as.call.args.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                emit_expr(gen, node->as.call.args.nodes[i]);
            }
            emit(gen, ")");
        }
        break;
    }

    case NODE_INDEX:
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
        break;

    case NODE_SLICE: {
        // Slice produces a span: (__Span_T){ .data = ..., .count = ... }
        Type* span_type = node->as.slice.resolved_type;
        Type* elem_type = span_type->as.span.elem;

        // Emit compound literal
        emit(gen, "((__Span_%s){ .data = ", type_name(elem_type));

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
        break;
    }

    case NODE_MEMBER: {
        // Check if this is module-qualified access (already handled struct_name case)
        if (node->as.member.struct_name == NULL && node->as.member.module_name == NULL) {
            // Check if object is 'self' - always a pointer in methods
            // This handles generic methods where is_ref isn't set because body isn't type-checked
            int is_self = (node->as.member.object->type == NODE_IDENT &&
                           strcmp(node->as.member.object->as.ident.name, "self") == 0);

            if (node->as.member.is_ref || is_self) {
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
        break;
    }

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

    case NODE_NEW_EXPR: {
        Type* rtype = node->as.new_expr.resolved_type;
        // In generic method bodies, the checker doesn't visit the body, so resolved_type
        // may be NULL. Resolve from the type_node using the current substitution context.
        if (!rtype) {
            Node* tn = node->as.new_expr.type_node;
            if (tn->type == NODE_GENERIC_TYPE) {
                if (strcmp(tn->as.generic_type.base_name, "Vec") == 0) {
                    // Look up the Vec instance by mangled name
                    char* mangled = build_mangled_name_from_generic_node(gen, tn);
                    for (int vi = 0; vi < gen->vec_instance_count; vi++) {
                        if (strcmp(gen->vec_instances[vi].mangled_name, mangled) == 0) {
                            rtype = gen->vec_instances[vi].type;
                            break;
                        }
                    }
                    free(mangled);
                } else {
                    // Look up the generic struct instance by mangled name
                    char* mangled = build_mangled_name_from_generic_node(gen, tn);
                    for (int gi = 0; gi < gen->generic_instance_count; gi++) {
                        if (strcmp(gen->generic_instances[gi].mangled_name, mangled) == 0) {
                            rtype = gen->generic_instances[gi].type;
                            break;
                        }
                    }
                    free(mangled);
                }
            } else if (tn->type == NODE_IDENT) {
                // Simple type name — check substitution context first
                const char* name = tn->as.ident.name;
                if (gen->subst_ctx) {
                    for (int si = 0; si < gen->subst_ctx->count; si++) {
                        if (strcmp(gen->subst_ctx->type_params[si], name) == 0) {
                            rtype = gen->subst_ctx->type_args[si];
                            break;
                        }
                    }
                }
            }
        }
        if (rtype->kind == TYPE_VEC) {
            // new Vec<T>{elems} as inline expression using GCC statement expression
            const char* elem_tname = type_name(rtype->as.vec.elem);
            int         tmp        = gen->temp_count++;
            emit(gen,
                 "({ __Vec_%s* __rc_tmp%d = (__Vec_%s*)__rc_alloc(sizeof(__Vec_%s)); "
                 "__rc_tmp%d->data = NULL; __rc_tmp%d->count = 0; __rc_tmp%d->capacity = 0;",
                 elem_tname, tmp, elem_tname, elem_tname, tmp, tmp, tmp);
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
        } else {
            // new Type { fields } as inline expression using GCC statement expression
            const char* tname = rtype->as.struc.name;
            int         tmp   = gen->temp_count++;
            emit(gen, "({ %s* __rc_tmp%d = (%s*)__rc_alloc(sizeof(%s)); *__rc_tmp%d = (%s)", tname,
                 tmp, tname, tname, tmp, tname);
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
        break;
    }

    default:
        emit(gen, "/* unknown expr %d */", node->type);
        break;
    }
}

static void emit_struct_init(CodeGen* gen, Node* node) {
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
static void emit_destruct_pattern(CodeGen* gen, DestructPattern* pattern, const char* temp_prefix,
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
                int temp_id = gen->temp_count++;
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
    }
}

// Emit statements within a block without emitting braces, but with RC scope handling
static void emit_block_contents(CodeGen* gen, Node* block) {
    if (!block || block->type != NODE_BLOCK)
        return;

    gen->rc_scope_depth++;
    for (int i = 0; i < block->as.block.stmts.count; i++) {
        emit_stmt(gen, block->as.block.stmts.nodes[i]);
    }
    rc_cleanup_scope(gen, gen->rc_scope_depth);
    gen->rc_scope_depth--;
}

void emit_stmt(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT: {
        // Handle RC reassignment: p = new_value
        // Must inc new value, dec old value, then assign
        Node* expr = node->as.expr_stmt.expr;
        if (expr->type == NODE_ASSIGN && expr->as.assign.op == TOK_EQ &&
            expr->as.assign.target->type == NODE_IDENT &&
            rc_is_tracked(gen, expr->as.assign.target->as.ident.name)) {
            const char* var_name = expr->as.assign.target->as.ident.name;
            Type*       var_type = rc_get_var_type(gen, var_name);
            if (var_type && var_type->kind == TYPE_ENUM && var_type->as.enm.has_rc_fields) {
                // Enum value reassignment: dec old payload, then assign, then inc if copying
                int temp_id = gen->temp_count++;
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
                break;
            }

            // Evaluate new value into a temp (in case it references the old value)
            int temp_id = gen->temp_count++;
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
            break;
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
                        if (strcmp(obj_type->as.struc.field_names[f], member->as.member.name) ==
                            0) {
                            field_ty = obj_type->as.struc.field_types[f];
                            break;
                        }
                    }
                }
                if (field_ty && field_ty->kind == TYPE_ENUM && field_ty->as.enm.has_rc_fields) {
                    int temp_id = gen->temp_count++;
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
                    break;
                }

                int value_is_rc = (expr->as.assign.value->type == NODE_IDENT &&
                                   rc_is_tracked(gen, expr->as.assign.value->as.ident.name)) ||
                                  expr->as.assign.value->type == NODE_NEW_EXPR;
                if (value_is_rc && field_ty && field_ty->kind == TYPE_STRUCT) {
                    // Determine the dec function for the field's type
                    char*       member_dec_owned = (char*)get_dec_func_for_type(field_ty);
                    const char* member_dec       = member_dec_owned;
                    int         tmp              = gen->temp_count++;
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
                    break;
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
            break;
        }
        emit_indent(gen);
        emit_expr(gen, expr);
        emit(gen, ";\n");
        break;
    }

    case NODE_VAR_DECL: {
        // Handle destructuring: var (a, b) = tuple; or var (a, (b, c)) = nested;
        DestructPattern* pattern = node->as.var_decl.destruct_pattern;
        if (pattern) {
            Type* tuple_type = pattern->resolved_type;

            // Emit a temporary variable to hold the tuple value
            emit_indent(gen);
            emit_resolved_type(gen, tuple_type);
            int temp_id = gen->temp_count++;
            emit(gen, " __tuple%d = ", temp_id);
            emit_expr(gen, node->as.var_decl.init);
            emit(gen, ";\n");

            // Recursively emit the destructured variables
            char temp_prefix[64];
            snprintf(temp_prefix, sizeof(temp_prefix), "__tuple%d", temp_id);
            emit_destruct_pattern(gen, pattern, temp_prefix, node->as.var_decl.is_const);
            break;
        }

        // Handle RC-managed variable declarations
        if (node->as.var_decl.is_rc && node->as.var_decl.init) {
            if (node->as.var_decl.init->type == NODE_NEW_EXPR) {
                Type* rtype = node->as.var_decl.init->as.new_expr.resolved_type;
                if (rtype && rtype->kind == TYPE_VEC) {
                    // var v = new Vec<T>{} or new Vec<T>{1, 2, 3}
                    const char* elem_tname = type_name(rtype->as.vec.elem);
                    emit_indent(gen);
                    emit(gen, "__Vec_%s* %s = (__Vec_%s*)__rc_alloc(sizeof(__Vec_%s));\n",
                         elem_tname, node->as.var_decl.name, elem_tname, elem_tname);
                    emit_indent(gen);
                    emit(gen, "%s->data = NULL; %s->count = 0; %s->capacity = 0;\n",
                         node->as.var_decl.name, node->as.var_decl.name, node->as.var_decl.name);
                    // Push initial elements
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
                    break;
                }
                // var p = new Point { x: 1, y: 2 }
                // => Point* p = (Point*)__rc_alloc(sizeof(Point));
                //    *p = (Point){ .x = 1, .y = 2 };
                const char* tname = rtype->as.struc.name;
                emit_indent(gen);
                emit(gen, "%s* %s = (%s*)__rc_alloc(sizeof(%s));\n", tname, node->as.var_decl.name,
                     tname, tname);
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
                break;
            } else {
                // RC copy or ownership transfer from function call
                emit_indent(gen);
                if (node->as.var_decl.type) {
                    emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
                } else if (node->as.var_decl.resolved_type) {
                    // Use resolved struct type from checker
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
                Type* rc_type  = node->as.var_decl.resolved_type;
                int   skip_inc = node->as.var_decl.init->type == NODE_CALL ||
                               (rc_type && rc_type->kind == TYPE_ENUM &&
                                node->as.var_decl.init->type == NODE_ENUM_VALUE);
                if (!skip_inc && rc_type) {
                    // Copy of existing RC var: increment refcount
                    const char* inc_fn = get_inc_func_for_type(rc_type);
                    emit_indent(gen);
                    emit(gen, "%s(%s);\n", inc_fn, node->as.var_decl.name);
                    free((char*)inc_fn);
                }
                const char* dec_fn2 = get_dec_func_for_type(rc_type);
                rc_push_var(gen, node->as.var_decl.name, dec_fn2, rc_type);
                free((char*)dec_fn2);
                break;
            }
        }

        emit_indent(gen);
        if (node->as.var_decl.is_const) {
            emit(gen, "const ");
        }

        // Check if this is a struct type variable with initializer
        int struct_type = node->as.var_decl.type && is_struct_type(gen, node->as.var_decl.type);

        if (node->as.var_decl.type) {
            emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
        } else {
            // Type inference - use auto or infer from init
            // For C, we need to determine the type from the initializer
            // For simplicity, use int64_t for int literals, float for f32 literals
            if (node->as.var_decl.init) {
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
                    // Build a tuple type from the literal's elements
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
                            // Nested tuple - would need recursive handling
                            elems[i] = type_int64; // Fallback
                            break;
                        default:
                            elems[i] = type_int64; // Default
                            break;
                        }
                    }
                    Type* tuple = type_tuple(elems, count);
                    // Find matching typedef
                    int idx = -1;
                    for (int i = 0; i < gen->tuple_type_count; i++) {
                        if (tuple_types_equal(gen->tuple_types[i], tuple)) {
                            idx = i;
                            break;
                        }
                    }
                    if (idx >= 0) {
                        emit(gen, "__tuple_t%d %s", idx, node->as.var_decl.name);
                    } else {
                        // Fallback to inline struct
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
                    // Use the resolved element type from checker
                    Node* init      = node->as.var_decl.init;
                    Type* elem_type = init->as.array_lit.resolved_type;
                    int   count     = init->as.array_lit.elements.count;
                    emit_resolved_type(gen, elem_type);
                    emit(gen, " %s[%d]", node->as.var_decl.name, count);
                    break;
                }
                case NODE_ENUM_VALUE:
                    // Emit enum type name from the initializer
                    emit(gen, "%.*s %s", node->as.var_decl.init->as.enum_value.enum_name_length,
                         node->as.var_decl.init->as.enum_value.enum_name, node->as.var_decl.name);
                    break;
                default:
                    // Default to auto if we can't determine
                    emit(gen, "int64_t %s", node->as.var_decl.name);
                    break;
                }
            } else {
                emit(gen, "int64_t %s", node->as.var_decl.name);
            }
        }
        if (node->as.var_decl.init) {
            if (struct_type && node->as.var_decl.init->type == NODE_NULL_LIT) {
                // Struct type with null: just assign NULL
                emit(gen, " = NULL");
            } else {
                emit(gen, " = ");
                emit_expr(gen, node->as.var_decl.init);
            }
        }
        emit(gen, ";\n");
        break;
    }

    case NODE_BLOCK:
        emit_indent(gen);
        emit(gen, "{\n");
        gen->indent++;
        gen->rc_scope_depth++;
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            emit_stmt(gen, node->as.block.stmts.nodes[i]);
        }
        rc_cleanup_scope(gen, gen->rc_scope_depth);
        gen->rc_scope_depth--;
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

    case NODE_IF:
        emit_indent(gen);
        emit(gen, "if (");
        emit_expr(gen, node->as.if_stmt.cond);
        emit(gen, ") {\n");
        gen->indent++;
        // Emit then block contents directly (it's already a block)
        if (node->as.if_stmt.then_block->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.if_stmt.then_block);
        } else {
            emit_stmt(gen, node->as.if_stmt.then_block);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}");

        if (node->as.if_stmt.else_block) {
            if (node->as.if_stmt.else_block->type == NODE_IF) {
                // else if
                emit(gen, " else ");
                // Remove indent for else if
                gen->indent--;
                emit_stmt(gen, node->as.if_stmt.else_block);
                gen->indent++;
            } else {
                emit(gen, " else {\n");
                gen->indent++;
                if (node->as.if_stmt.else_block->type == NODE_BLOCK) {
                    emit_block_contents(gen, node->as.if_stmt.else_block);
                } else {
                    emit_stmt(gen, node->as.if_stmt.else_block);
                }
                gen->indent--;
                emit_indent(gen);
                emit(gen, "}\n");
            }
        } else {
            emit(gen, "\n");
        }
        break;

    case NODE_WHILE:
        emit_indent(gen);
        emit(gen, "while (");
        emit_expr(gen, node->as.while_stmt.cond);
        emit(gen, ") {\n");
        gen->indent++;
        if (node->as.while_stmt.body->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.while_stmt.body);
        } else {
            emit_stmt(gen, node->as.while_stmt.body);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

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
        gen->indent++;
        if (node->as.for_stmt.body->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.for_stmt.body);
        } else {
            emit_stmt(gen, node->as.for_stmt.body);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;

    case NODE_FOREACH:
        emit_indent(gen);
        // Generate: for (int64_t var = start; var <= end; var += step) {
        emit(gen, "for (int64_t %s = ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.start);
        emit(gen, "; %s <= ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.end);
        emit(gen, "; %s += ", node->as.foreach_stmt.var_name);
        emit_expr(gen, node->as.foreach_stmt.step);
        emit(gen, ") {\n");
        gen->indent++;
        if (node->as.foreach_stmt.body->type == NODE_BLOCK) {
            emit_block_contents(gen, node->as.foreach_stmt.body);
        } else {
            emit_stmt(gen, node->as.foreach_stmt.body);
        }
        gen->indent--;
        emit_indent(gen);
        emit(gen, "}\n");
        break;
    case NODE_RETURN: {
        // Determine if we're returning an RC var (skip it in cleanup)
        const char* skip_name = NULL;
        if (node->as.return_stmt.value && node->as.return_stmt.value->type == NODE_IDENT) {
            // Check if the returned identifier is an RC var
            const char* ret_name = node->as.return_stmt.value->as.ident.name;
            for (int i = 0; i < gen->rc_var_count; i++) {
                if (strcmp(gen->rc_vars[i].name, ret_name) == 0) {
                    skip_name = ret_name;
                    break;
                }
            }
        }

        if (gen->defer_count > 0) {
            // With defers: store value in __ret, cleanup RC, goto cleanup
            emit_indent(gen);
            if (node->as.return_stmt.value) {
                emit(gen, "__ret = ");
                emit_expr(gen, node->as.return_stmt.value);
                emit(gen, ";\n");
            }
            if (gen->rc_var_count > 0) {
                rc_cleanup_all(gen, skip_name);
            }
            emit_indent(gen);
            emit(gen, "goto __cleanup;\n");
        } else {
            // No defers: cleanup RC, then return
            if (gen->rc_var_count > 0) {
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
        break;
    }

    case NODE_DEFER:
        // Don't emit anything here - just push to defer stack
        defer_push(gen, node->as.defer_stmt.stmt);
        break;

    case NODE_BREAK:
        rc_cleanup_scope(gen, gen->rc_scope_depth);
        emit_indent(gen);
        emit(gen, "break;\n");
        break;

    case NODE_CONTINUE:
        rc_cleanup_scope(gen, gen->rc_scope_depth);
        emit_indent(gen);
        emit(gen, "continue;\n");
        break;

    default:
        emit_indent(gen);
        emit(gen, "/* unknown stmt %d */;\n", node->type);
        break;
    }
}

void emit_decl(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXTERN_MODULE:
        emit(gen, "\n#include <%s.h>\n", node->as.extern_module.module_name);
        break;

    case NODE_STRUCT_DECL:
        // Struct body typedefs are emitted in codegen_emit before __rc_dec_TypeName
        break;

    case NODE_ENUM_DECL:
        // Enum typedefs are emitted in the early pass in codegen_emit
        break;

    case NODE_TRAIT_DECL:
        // Traits produce no C code - they are purely a type-checking concept
        break;

    case NODE_TYPE_ALIAS:
        // Type aliases produce no C code - they are resolved during type checking
        break;

    case NODE_IMPL_DECL:
        // Emit each method in the impl block as a regular function
        for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
            emit_decl(gen, node->as.impl_decl.methods.nodes[i]);
        }
        break;

    case NODE_FUNC_DECL: {
        int is_method = (node->as.func_decl.receiver_type != NULL);

        // Skip generic method templates - they get instantiated separately
        if (is_method && node->as.func_decl.receiver_type_args.count > 0) {
            break;
        }

        // Check if function is void
        int is_void = !node->as.func_decl.return_type ||
                      (node->as.func_decl.return_type->type == NODE_IDENT &&
                       strcmp(node->as.func_decl.return_type->as.ident.name, "void") == 0);

        // Emit static for private functions (except main)
        if (!node->as.func_decl.is_public && strcmp(node->as.func_decl.name, "main") != 0) {
            emit(gen, "static ");
        }

        // Return type
        emit_type(gen, node->as.func_decl.return_type);

        // Function name (mangled for methods and library functions)
        emit_function_name(gen, node->as.func_decl.name,
                           is_method ? node->as.func_decl.receiver_type : NULL,
                           gen->current_module);

        // Parameters
        if (is_method) {
            // Emit self parameter first
            if (node->as.func_decl.receiver_is_const) {
                emit(gen, "const ");
            }
            emit(gen, "%s* self", node->as.func_decl.receiver_type);
            if (node->as.func_decl.params.count > 0) {
                emit(gen, ", ");
            }
        }

        if (node->as.func_decl.params.count == 0 && !is_method) {
            emit(gen, "void");
        } else {
            for (int i = 0; i < node->as.func_decl.params.count; i++) {
                if (i > 0)
                    emit(gen, ", ");
                Node* param = node->as.func_decl.params.nodes[i];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
        }
        emit(gen, ") {\n");

        // Clear defer stack for this function
        defer_clear(gen);
        gen->current_return_type = node->as.func_decl.return_type;

        // First pass: count defers to know if we need __ret
        int has_defers = 0;
        if (node->as.func_decl.body) {
            for (int i = 0; i < node->as.func_decl.body->as.block.stmts.count; i++) {
                Node* stmt = node->as.func_decl.body->as.block.stmts.nodes[i];
                if (stmt && stmt->type == NODE_DEFER) {
                    has_defers = 1;
                    break;
                }
            }
        }

        // Body
        gen->indent++;

        // Declare __ret if function has defers and is non-void
        if (has_defers && !is_void) {
            emit_indent(gen);
            emit_type(gen, node->as.func_decl.return_type);
            emit(gen, " __ret;\n");
        }

        if (node->as.func_decl.body) {
            for (int i = 0; i < node->as.func_decl.body->as.block.stmts.count; i++) {
                emit_stmt(gen, node->as.func_decl.body->as.block.stmts.nodes[i]);
            }
        }

        // Emit cleanup section if there are defers
        if (gen->defer_count > 0) {
            emit(gen, "__cleanup:;\n");
            // Emit deferred statements in reverse order (LIFO)
            for (int i = gen->defer_count - 1; i >= 0; i--) {
                emit_stmt(gen, gen->defer_stack[i]);
            }
            // Emit final return
            emit_indent(gen);
            if (is_void) {
                emit(gen, "return;\n");
            } else {
                emit(gen, "return __ret;\n");
            }
        }

        gen->indent--;
        emit(gen, "}\n\n");

        // Clear defer stack and RC tracking
        defer_clear(gen);
        rc_clear_all(gen);
        gen->current_return_type = NULL;
        break;
    }

    case NODE_VAR_DECL:
        // Global variable - emit static for private vars
        if (!node->as.var_decl.is_public) {
            emit(gen, "static ");
        }
        emit_stmt(gen, node);
        emit(gen, "\n");
        break;

    default:
        emit(gen, "/* unknown decl %d */\n", node->type);
        break;
    }
}
