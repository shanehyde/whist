#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "types.h"
#include "vec.h"

static void emit_indent(CodeGen* gen) {
    for (int i = 0; i < gen->indent; i++) {
        fprintf(gen->out, "    ");
    }
}

// Forward declaration for recursive pattern emit
static void emit_destruct_pattern(CodeGen* gen, DestructPattern* pattern, const char* temp_prefix,
                                  int is_const);

static void emit(CodeGen* gen, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out, fmt, args);
    va_end(args);
}

static void  emit_type(CodeGen* gen, Node* type_node);
static void  emit_resolved_type(CodeGen* gen, Type* type);
static void  emit_expr(CodeGen* gen, Node* node);
static void  emit_stmt(CodeGen* gen, Node* node);
static void  emit_decl(CodeGen* gen, Node* node);
static void  emit_struct_init(CodeGen* gen, Node* node);
static int   tuple_types_equal(Type* a, Type* b);
static Type* type_from_node(Node* type_node);
static void  emit_block_contents(CodeGen* gen, Node* block);
static char* build_mangled_name_from_generic_node(CodeGen* gen, Node* type_node);

static void defer_push(CodeGen* gen, Node* node) {
    VEC_GROW(gen->defer_stack, gen->defer_count, gen->defer_capacity);
    gen->defer_stack[gen->defer_count++] = node;
}

static void defer_clear(CodeGen* gen) {
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
static void rc_clear_all(CodeGen* gen) {
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
static int is_enum_type_name(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enum_name_count; i++) {
        if (strcmp(gen->enum_names[i], name) == 0)
            return 1;
    }
    return 0;
}

static int enum_index(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enum_name_count; i++) {
        if (strcmp(gen->enum_names[i], name) == 0)
            return i;
    }
    return -1;
}

static int enum_has_rc_fields(CodeGen* gen, const char* name) {
    int idx = enum_index(gen, name);
    if (idx < 0 || !gen->enum_has_rc_fields)
        return 0;
    return gen->enum_has_rc_fields[idx];
}

// Check if a type node represents a struct (user-defined) type
static int is_struct_type(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return 0;
    // Generic types (Box<i64>) are always struct types, except Span<T> and generic enums
    if (type_node->type == NODE_GENERIC_TYPE) {
        if (strcmp(type_node->as.generic_type.base_name, "Span") == 0) {
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

static int type_node_has_rc(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return 0;
    if (type_node->type == NODE_GENERIC_TYPE) {
        if (strcmp(type_node->as.generic_type.base_name, "Span") == 0)
            return 0;
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
static const char* resolve_enum_name(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return NULL;
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
static int codegen_is_type_variable(const char* name) {
    return !type_is_builtin_name(name);
}

// Extract additional type bindings from matching a method's receiver pattern against concrete args
// For example, matching <i32, Box<T>> against [i32, Box_i64] extracts T=i64
// Returns 1 on match (and fills out_* params), 0 on no match
static int codegen_extract_method_bindings(NodeList* pattern_args, Type** concrete_args,
                                           int arg_count, char*** out_params, Type*** out_args,
                                           int* out_count) {
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
static void emit_function_name(CodeGen* gen, const char* func_name, const char* receiver_type,
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
static void emit_type(CodeGen* gen, Node* type_node) {
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
            // User-defined struct type - emit as pointer (struct references)
            emit(gen, "%s*", name);
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
static void emit_resolved_type(CodeGen* gen, Type* type) {
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
static void emit_type_with_name(CodeGen* gen, Node* type_node, const char* name) {
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
        if (node->as.index.is_span_index) {
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
        Type* span_type = (Type*)node->as.slice.resolved_type;
        Type* elem_type = span_type->as.span.elem;

        // Emit compound literal
        emit(gen, "((__Span_%s){ .data = ", type_name(elem_type));

        if (node->as.slice.is_array) {
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
        // For omitted end, use array length or span.count
        if (node->as.slice.end) {
            emit(gen, "(");
            emit_expr(gen, node->as.slice.end);
            emit(gen, ")");
        } else {
            // Use full length
            if (node->as.slice.is_array) {
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
        // new Type { fields } as inline expression using GCC statement expression
        Type*       stype = (Type*)node->as.new_expr.resolved_type;
        const char* tname = stype->as.struc.name;
        int         tmp   = gen->temp_count++;
        emit(gen, "({ %s* __rc_tmp%d = (%s*)__rc_alloc(sizeof(%s)); *__rc_tmp%d = (%s)", tname, tmp,
             tname, tname, tmp, tname);
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

    Type* type = (Type*)pattern->resolved_type;

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
                Type* elem_type = (Type*)elem->resolved_type;
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

static void emit_stmt(CodeGen* gen, Node* node) {
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
        emit_indent(gen);
        emit_expr(gen, expr);
        emit(gen, ";\n");
        break;
    }

    case NODE_VAR_DECL: {
        // Handle destructuring: var (a, b) = tuple; or var (a, (b, c)) = nested;
        DestructPattern* pattern = node->as.var_decl.destruct_pattern;
        if (pattern) {
            Type* tuple_type = (Type*)pattern->resolved_type;

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
                // var p = new Point { x: 1, y: 2 }
                // => Point* p = (Point*)__rc_alloc(sizeof(Point));
                //    *p = (Point){ .x = 1, .y = 2 };
                Type*       stype = (Type*)node->as.var_decl.init->as.new_expr.resolved_type;
                const char* tname = stype->as.struc.name;
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
                const char* dec_fn = get_dec_func_for_type(stype);
                rc_push_var(gen, node->as.var_decl.name, dec_fn, stype);
                free((char*)dec_fn);
                break;
            } else {
                // RC copy or ownership transfer from function call
                emit_indent(gen);
                if (node->as.var_decl.type) {
                    emit_type_with_name(gen, node->as.var_decl.type, node->as.var_decl.name);
                } else if (node->as.var_decl.resolved_type) {
                    // Use resolved struct type from checker
                    Type* rtype = (Type*)node->as.var_decl.resolved_type;
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
                Type* rc_type  = (Type*)node->as.var_decl.resolved_type;
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
                    Type* elem_type = (Type*)init->as.array_lit.resolved_type;
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

static void emit_decl(CodeGen* gen, Node* node) {
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

// Check if two tuple types are structurally equal
static int tuple_types_equal(Type* a, Type* b) {
    if (a->as.tuple.elem_count != b->as.tuple.elem_count)
        return 0;
    for (int i = 0; i < a->as.tuple.elem_count; i++) {
        if (!type_equals(a->as.tuple.elem_types[i], b->as.tuple.elem_types[i]))
            return 0;
    }
    return 1;
}

// Register a tuple type and return its index (for typedef name)
static int register_tuple_type(CodeGen* gen, Type* type) {
    // Check if already registered
    for (int i = 0; i < gen->tuple_type_count; i++) {
        if (tuple_types_equal(gen->tuple_types[i], type))
            return i;
    }
    // Add new
    VEC_GROW(gen->tuple_types, gen->tuple_type_count, gen->tuple_type_capacity);
    gen->tuple_types[gen->tuple_type_count] = type;
    return gen->tuple_type_count++;
}

// Collect tuple types from a type node
static void collect_tuple_types_from_node(CodeGen* gen, Node* type_node);

// Build a Type* from a tuple literal (for type collection)
static Type* type_from_tuple_lit(Node* node) {
    int    count = node->as.tuple_lit.elements.count;
    Type** elems = xmalloc(count * sizeof(Type*));
    for (int i = 0; i < count; i++) {
        Node* elem = node->as.tuple_lit.elements.nodes[i];
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
            elems[i] = type_from_tuple_lit(elem);
            break;
        default:
            elems[i] = type_int64; // Default
            break;
        }
    }
    return type_tuple(elems, count);
}

// Collect tuple types from an expression node
static void collect_tuple_types_from_expr(CodeGen* gen, Node* node) {
    if (!node)
        return;
    switch (node->type) {
    case NODE_TUPLE_LIT: {
        // Build and register the tuple type
        Type* tuple = type_from_tuple_lit(node);
        register_tuple_type(gen, tuple);
        // Collect from nested tuples
        for (int i = 0; i < node->as.tuple_lit.elements.count; i++) {
            collect_tuple_types_from_expr(gen, node->as.tuple_lit.elements.nodes[i]);
        }
        break;
    }
    case NODE_BINARY:
        collect_tuple_types_from_expr(gen, node->as.binary.left);
        collect_tuple_types_from_expr(gen, node->as.binary.right);
        break;
    case NODE_UNARY:
        collect_tuple_types_from_expr(gen, node->as.unary.operand);
        break;
    case NODE_CALL:
        collect_tuple_types_from_expr(gen, node->as.call.func);
        for (int i = 0; i < node->as.call.args.count; i++) {
            collect_tuple_types_from_expr(gen, node->as.call.args.nodes[i]);
        }
        break;
    case NODE_INDEX:
        collect_tuple_types_from_expr(gen, node->as.index.object);
        collect_tuple_types_from_expr(gen, node->as.index.index);
        break;
    case NODE_MEMBER:
        collect_tuple_types_from_expr(gen, node->as.member.object);
        break;
    case NODE_ASSIGN:
        collect_tuple_types_from_expr(gen, node->as.assign.target);
        collect_tuple_types_from_expr(gen, node->as.assign.value);
        break;
    default:
        break;
    }
}

// Collect tuple types from a destructuring pattern (recursive)
static void collect_tuple_types_from_pattern(CodeGen* gen, DestructPattern* pattern) {
    if (!pattern)
        return;

    // Collect the resolved type if it's a tuple
    Type* type = (Type*)pattern->resolved_type;
    if (type && type->kind == TYPE_TUPLE) {
        register_tuple_type(gen, type);
    }

    // Recurse into nested patterns
    if (pattern->kind == PATTERN_TUPLE) {
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            collect_tuple_types_from_pattern(gen, pattern->as.tuple.elements[i]);
        }
    }
}

// Collect tuple types from a statement node
static void collect_tuple_types_from_stmt(CodeGen* gen, Node* node) {
    if (!node)
        return;
    switch (node->type) {
    case NODE_VAR_DECL:
        if (node->as.var_decl.type)
            collect_tuple_types_from_node(gen, node->as.var_decl.type);
        if (node->as.var_decl.init)
            collect_tuple_types_from_expr(gen, node->as.var_decl.init);
        // Also collect from destructuring pattern if present
        if (node->as.var_decl.destruct_pattern) {
            collect_tuple_types_from_pattern(gen, node->as.var_decl.destruct_pattern);
        }
        break;
    case NODE_EXPR_STMT:
        collect_tuple_types_from_expr(gen, node->as.expr_stmt.expr);
        break;
    case NODE_BLOCK:
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            collect_tuple_types_from_stmt(gen, node->as.block.stmts.nodes[i]);
        }
        break;
    case NODE_IF:
        collect_tuple_types_from_expr(gen, node->as.if_stmt.cond);
        collect_tuple_types_from_stmt(gen, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_block)
            collect_tuple_types_from_stmt(gen, node->as.if_stmt.else_block);
        break;
    case NODE_WHILE:
        collect_tuple_types_from_expr(gen, node->as.while_stmt.cond);
        collect_tuple_types_from_stmt(gen, node->as.while_stmt.body);
        break;
    case NODE_FOR:
        if (node->as.for_stmt.init)
            collect_tuple_types_from_stmt(gen, node->as.for_stmt.init);
        if (node->as.for_stmt.cond)
            collect_tuple_types_from_expr(gen, node->as.for_stmt.cond);
        if (node->as.for_stmt.post)
            collect_tuple_types_from_expr(gen, node->as.for_stmt.post);
        collect_tuple_types_from_stmt(gen, node->as.for_stmt.body);
        break;
    case NODE_RETURN:
        if (node->as.return_stmt.value)
            collect_tuple_types_from_expr(gen, node->as.return_stmt.value);
        break;
    case NODE_DEFER:
        collect_tuple_types_from_stmt(gen, node->as.defer_stmt.stmt);
        break;
    default:
        break;
    }
}

// Build a Type* from a type node (for codegen purposes, simplified)
static Type* type_from_node(Node* type_node) {
    if (!type_node)
        return type_void;

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name    = type_node->as.ident.name;
        Type*       builtin = type_builtin_from_name(name);
        if (builtin)
            return builtin;
        // User-defined type - return a struct type
        return type_struct(name);
    }
    case NODE_ARRAY_TYPE: {
        Type* elem = type_from_node(type_node->as.array_type.elem_type);
        int   size = -1;
        if (type_node->as.array_type.size && type_node->as.array_type.size->type == NODE_INT_LIT) {
            size = (int)type_node->as.array_type.size->as.int_lit.value;
        }
        return type_array(elem, size);
    }
    case NODE_TUPLE_TYPE: {
        int    count = type_node->as.tuple_type.elem_types.count;
        Type** elems = xmalloc(count * sizeof(Type*));
        for (int i = 0; i < count; i++) {
            elems[i] = type_from_node(type_node->as.tuple_type.elem_types.nodes[i]);
        }
        return type_tuple(elems, count);
    }
    case NODE_GENERIC_TYPE: {
        // For codegen, build a struct type with the mangled name
        // Build mangled name
        const char* base      = type_node->as.generic_type.base_name;
        int         arg_count = type_node->as.generic_type.type_args.count;
        Type**      args      = xmalloc(arg_count * sizeof(Type*));
        for (int i = 0; i < arg_count; i++) {
            args[i] = type_from_node(type_node->as.generic_type.type_args.nodes[i]);
        }
        char* mangled = type_mangle_generic(base, args, arg_count);
        Type* result  = type_struct(mangled);
        free(mangled);
        free(args);
        return result;
    }
    default:
        return type_error;
    }
}

// Forward declare for finding generic struct declarations
static Node* find_generic_struct_decl(Node* ast, const char* name);

// Collect tuple types from a type node
static void collect_tuple_types_from_node(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return;
    if (type_node->type == NODE_TUPLE_TYPE) {
        // Build a Type* and register it
        Type* tuple = type_from_node(type_node);
        register_tuple_type(gen, tuple);
        // Recurse into element types
        for (int i = 0; i < type_node->as.tuple_type.elem_types.count; i++) {
            collect_tuple_types_from_node(gen, type_node->as.tuple_type.elem_types.nodes[i]);
        }
    } else if (type_node->type == NODE_ARRAY_TYPE) {
        // Array type - recurse into element type
        collect_tuple_types_from_node(gen, type_node->as.array_type.elem_type);
    } else if (type_node->type == NODE_GENERIC_TYPE) {
        // Generic type - recurse into type arguments for any tuple types
        for (int i = 0; i < type_node->as.generic_type.type_args.count; i++) {
            collect_tuple_types_from_node(gen, type_node->as.generic_type.type_args.nodes[i]);
        }
        // Note: generic instances are passed from checker, no need to collect here
    }
}

// Find a generic struct declaration by name in the AST
static Node* find_generic_struct_decl(Node* ast, const char* name) {
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
static Node* find_generic_enum_decl(Node* ast, const char* name) {
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

// Build a mangled name from a NODE_GENERIC_TYPE node (with subst_ctx support) into a buffer.
// Returns a dynamically allocated string that the caller must free.
static char* build_mangled_name_from_generic_node(CodeGen* gen, Node* type_node) {
    char buf[256];
    int  pos = 0;
    pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", type_node->as.generic_type.base_name);
    for (int i = 0; i < type_node->as.generic_type.type_args.count; i++) {
        pos += snprintf(buf + pos, sizeof(buf) - pos, "_");
        Node* arg = type_node->as.generic_type.type_args.nodes[i];
        if (arg->type == NODE_IDENT) {
            const char* arg_name    = arg->as.ident.name;
            int         substituted = 0;
            if (gen->subst_ctx) {
                for (int s = 0; s < gen->subst_ctx->count; s++) {
                    if (strcmp(gen->subst_ctx->type_params[s], arg_name) == 0) {
                        Type* subst_type = gen->subst_ctx->type_args[s];
                        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", type_name(subst_type));
                        substituted = 1;
                        break;
                    }
                }
            }
            if (!substituted) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", arg_name);
            }
        } else if (arg->type == NODE_GENERIC_TYPE) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", arg->as.generic_type.base_name);
            for (int j = 0; j < arg->as.generic_type.type_args.count; j++) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "_");
                Node* nested = arg->as.generic_type.type_args.nodes[j];
                if (nested->type == NODE_IDENT) {
                    const char* nested_name = nested->as.ident.name;
                    int         subst       = 0;
                    if (gen->subst_ctx) {
                        for (int s = 0; s < gen->subst_ctx->count; s++) {
                            if (strcmp(gen->subst_ctx->type_params[s], nested_name) == 0) {
                                Type* subst_type = gen->subst_ctx->type_args[s];
                                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s",
                                                type_name(subst_type));
                                subst = 1;
                                break;
                            }
                        }
                    }
                    if (!subst) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", nested_name);
                    }
                }
            }
        }
    }
    return xstrdup(buf);
}

// Helper to get the type parameter name for mangling (currently unused)
// static const char* type_arg_mangle_name(Type* type) {
//     ... preserved for potential future use
// }

// Collect all generic methods for a given struct name
static void collect_generic_methods(Node* ast, const char* struct_name, Node*** methods_out,
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
            // Direct generic methods: func (Box<T>) get(): T
            if (decl->type == NODE_FUNC_DECL && decl->as.func_decl.receiver_type != NULL &&
                decl->as.func_decl.receiver_type_args.count > 0 &&
                strcmp(decl->as.func_decl.receiver_type, struct_name) == 0) {
                VEC_GROW(methods, count, capacity);
                methods[count++] = decl;
            }
            // Methods inside impl blocks: impl Drop for Box { func (Box<T>) drop(): void }
            if (decl->type == NODE_IMPL_DECL) {
                for (int j = 0; j < decl->as.impl_decl.methods.count; j++) {
                    Node* method = decl->as.impl_decl.methods.nodes[j];
                    if (method->type == NODE_FUNC_DECL &&
                        method->as.func_decl.receiver_type != NULL &&
                        method->as.func_decl.receiver_type_args.count > 0 &&
                        strcmp(method->as.func_decl.receiver_type, struct_name) == 0) {
                        VEC_GROW(methods, count, capacity);
                        methods[count++] = method;
                    }
                }
            }
        }
    }

    *methods_out = methods;
    *count_out   = count;
}

// Collect all tuple types from a declaration
static void collect_tuple_types_from_decl(CodeGen* gen, Node* decl) {
    if (!decl)
        return;
    switch (decl->type) {
    case NODE_FUNC_DECL: {
        // Skip generic method templates - they contain type variables
        // that shouldn't be instantiated as generic types
        int is_method = (decl->as.func_decl.receiver_type != NULL);
        if (is_method && decl->as.func_decl.receiver_type_args.count > 0) {
            break;
        }
        if (decl->as.func_decl.return_type)
            collect_tuple_types_from_node(gen, decl->as.func_decl.return_type);
        for (int i = 0; i < decl->as.func_decl.params.count; i++) {
            Node* param = decl->as.func_decl.params.nodes[i];
            if (param->as.param.type)
                collect_tuple_types_from_node(gen, param->as.param.type);
        }
        if (decl->as.func_decl.body)
            collect_tuple_types_from_stmt(gen, decl->as.func_decl.body);
        break;
    }
    case NODE_VAR_DECL:
        if (decl->as.var_decl.type)
            collect_tuple_types_from_node(gen, decl->as.var_decl.type);
        if (decl->as.var_decl.init)
            collect_tuple_types_from_expr(gen, decl->as.var_decl.init);
        break;
    case NODE_STRUCT_DECL:
        for (int i = 0; i < decl->as.struct_decl.fields.count; i++) {
            Node* field = decl->as.struct_decl.fields.nodes[i];
            if (field->as.field.type)
                collect_tuple_types_from_node(gen, field->as.field.type);
        }
        break;
    case NODE_IMPL_DECL:
        for (int i = 0; i < decl->as.impl_decl.methods.count; i++) {
            collect_tuple_types_from_decl(gen, decl->as.impl_decl.methods.nodes[i]);
        }
        break;
    default:
        break;
    }
}

void codegen_init(CodeGen* gen, FILE* out, GenericInstance* generic_instances, int generic_count,
                  SpanInstance* span_instances, int span_count, TraitImpl* trait_impls,
                  int trait_impl_count, int rc_debug) {
    gen->out                    = out;
    gen->indent                 = 0;
    gen->temp_count             = 0;
    gen->defer_stack            = NULL;
    gen->defer_count            = 0;
    gen->defer_capacity         = 0;
    gen->current_return_type    = NULL;
    gen->current_module         = NULL;
    gen->rc_vars                = NULL;
    gen->rc_var_count           = 0;
    gen->rc_var_capacity        = 0;
    gen->rc_scope_depth         = 0;
    gen->rc_debug               = rc_debug;
    gen->subst_ctx              = NULL;
    gen->tuple_types            = NULL;
    gen->tuple_type_count       = 0;
    gen->tuple_type_capacity    = 0;
    gen->generic_instances      = generic_instances;
    gen->generic_instance_count = generic_count;
    gen->span_instances         = span_instances;
    gen->span_instance_count    = span_count;
    gen->trait_impls            = trait_impls;
    gen->trait_impl_count       = trait_impl_count;
    gen->enum_names             = NULL;
    gen->enum_name_count        = 0;
    gen->enum_name_capacity     = 0;
    gen->enum_has_rc_fields     = NULL;
}

void codegen_emit(CodeGen* gen, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM)
        return;

    // First pass: collect all tuple types
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            collect_tuple_types_from_decl(gen, mod->as.module.decls.nodes[i]);
        }
    }

    // Emit header
    emit(gen, "/* Generated by whist compiler */\n");
    emit(gen, "#include <stdint.h>\n");
    emit(gen, "#include <stdbool.h>\n");
    emit(gen, "#include <stddef.h>\n");
    emit(gen, "#include <stdlib.h>\n");
    emit(gen, "#include <stdio.h>\n");
    emit(gen, "\n");

    // Emit RC runtime helpers
    emit(gen, "typedef struct { size_t refcount; } __RcHeader;\n\n");
    if (gen->rc_debug) {
        emit(gen, "static inline void* __rc_alloc(size_t size) {\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)malloc(sizeof(__RcHeader) + size);\n");
        emit(gen, "    if (!h) { fprintf(stderr, \"Panic: out of memory\\n\"); exit(1); }\n");
        emit(gen, "    h->refcount = 1;\n");
        emit(gen, "    void* ptr = (void*)(h + 1);\n");
        emit(gen, "    fprintf(stderr, \"RC_ALLOC: %%p (size=%%zu, rc=1)\\n\", ptr, size);\n");
        emit(gen, "    return ptr;\n");
        emit(gen, "}\n\n");
        emit(gen, "static inline void __rc_inc(void* ptr) {\n");
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = ((__RcHeader*)ptr - 1);\n");
        emit(gen, "    h->refcount++;\n");
        emit(gen, "    fprintf(stderr, \"RC_INC: %%p (rc=%%zu)\\n\", ptr, h->refcount);\n");
        emit(gen, "}\n\n");
        emit(gen, "static inline void __rc_dec(void* ptr) {\n");
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
        emit(gen, "    if (--h->refcount == 0) {\n");
        emit(gen, "        fprintf(stderr, \"RC_FREE: %%p\\n\", ptr);\n");
        emit(gen, "        free(h);\n");
        emit(gen, "    } else {\n");
        emit(gen, "        fprintf(stderr, \"RC_DEC: %%p (rc=%%zu)\\n\", ptr, h->refcount);\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");
    } else {
        emit(gen, "static inline void* __rc_alloc(size_t size) {\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)malloc(sizeof(__RcHeader) + size);\n");
        emit(gen, "    if (!h) { fprintf(stderr, \"Panic: out of memory\\n\"); exit(1); }\n");
        emit(gen, "    h->refcount = 1;\n");
        emit(gen, "    return (void*)(h + 1);\n");
        emit(gen, "}\n\n");
        emit(gen, "static inline void __rc_inc(void* ptr) {\n");
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    ((__RcHeader*)ptr - 1)->refcount++;\n");
        emit(gen, "}\n\n");
        emit(gen, "static inline void __rc_dec(void* ptr) {\n");
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
        emit(gen, "    if (--h->refcount == 0) free(h);\n");
        emit(gen, "}\n\n");
    }

    // Emit span bounds check helper (only if we have span instances)
    if (gen->span_instance_count > 0) {
        emit(gen, "static inline void __w0_span_check(uint64_t count, int64_t idx, int line, int "
                  "col) {\n");
        emit(gen, "    if (idx < 0 || (uint64_t)idx >= count) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: span index %%lld out of bounds (count=%%llu) "
                  "at %%d:%%d\\n\",\n");
        emit(gen, "                (long long)idx, (unsigned long long)count, line, col);\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");
    }

    // Emit span struct typedefs
    for (int i = 0; i < gen->span_instance_count; i++) {
        SpanInstance* inst = &gen->span_instances[i];
        emit(gen, "typedef struct {\n");
        emit(gen, "    const ");
        emit_resolved_type(gen, inst->elem_type);
        emit(gen, "* data;\n");
        emit(gen, "    uint64_t count;\n");
        emit(gen, "} __Span_%s;\n\n", type_name(inst->elem_type));
    }

    // Emit tuple typedefs
    for (int i = 0; i < gen->tuple_type_count; i++) {
        emit(gen, "typedef struct { ");
        Type* tuple = gen->tuple_types[i];
        for (int j = 0; j < tuple->as.tuple.elem_count; j++) {
            emit_resolved_type(gen, tuple->as.tuple.elem_types[j]);
            emit(gen, " _%d; ", j);
        }
        emit(gen, "} __tuple_t%d;\n", i);
    }
    if (gen->tuple_type_count > 0)
        emit(gen, "\n");

    // Register enum names for type emission (skip generic templates)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue; // Skip generic enum templates
            VEC_GROW(gen->enum_names, gen->enum_name_count, gen->enum_name_capacity);
            gen->enum_names[gen->enum_name_count++] = xstrdup(decl->as.enum_decl.name);
        }
    }

    // Register generic enum instance names
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* inst = &gen->generic_instances[i];
        if (inst->type->kind != TYPE_ENUM)
            continue;
        VEC_GROW(gen->enum_names, gen->enum_name_count, gen->enum_name_capacity);
        gen->enum_names[gen->enum_name_count++] = xstrdup(inst->mangled_name);
    }

    // Compute enum RC payload flags (fixed-point to support enum-to-enum payloads)
    if (gen->enum_name_count > 0) {
        gen->enum_has_rc_fields = xcalloc(gen->enum_name_count, sizeof(int));
        int changed             = 1;
        while (changed) {
            changed = 0;
            for (int m = 0; m < ast->as.program.modules.count; m++) {
                Node* mod = ast->as.program.modules.nodes[m];
                if (!mod || mod->type != NODE_MODULE)
                    continue;
                for (int i = 0; i < mod->as.module.decls.count; i++) {
                    Node* decl = mod->as.module.decls.nodes[i];
                    if (decl->type != NODE_ENUM_DECL)
                        continue;
                    if (decl->as.enum_decl.type_param_count > 0)
                        continue; // Skip generic templates
                    int idx = enum_index(gen, decl->as.enum_decl.name);
                    if (idx < 0 || gen->enum_has_rc_fields[idx])
                        continue;
                    for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
                        Node* var = decl->as.enum_decl.values.nodes[v];
                        for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                            if (type_node_has_rc(gen, var->as.enum_variant.types.nodes[t])) {
                                gen->enum_has_rc_fields[idx] = 1;
                                changed                      = 1;
                                break;
                            }
                        }
                        if (gen->enum_has_rc_fields[idx])
                            break;
                    }
                }
            }
        }
        // Set RC flags for generic enum instances from checker Type data
        for (int i = 0; i < gen->generic_instance_count; i++) {
            GenericInstance* inst = &gen->generic_instances[i];
            if (inst->type->kind != TYPE_ENUM)
                continue;
            int idx = enum_index(gen, inst->mangled_name);
            if (idx >= 0 && inst->type->as.enm.has_rc_fields) {
                gen->enum_has_rc_fields[idx] = 1;
            }
        }
    }

    // Forward declarations for structs (skip generic templates)
    // Must come before enum typedefs since data enums may reference struct types
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL) {
                // Skip generic struct templates
                if (decl->as.struct_decl.type_param_count > 0) {
                    continue;
                }
                emit(gen, "typedef struct %s %s;\n", decl->as.struct_decl.name,
                     decl->as.struct_decl.name);
            }
        }
    }
    // Forward declarations for instantiated generic structs (skip enum instances)
    for (int i = 0; i < gen->generic_instance_count; i++) {
        if (gen->generic_instances[i].type->kind == TYPE_ENUM)
            continue;
        emit(gen, "typedef struct %s %s;\n", gen->generic_instances[i].mangled_name,
             gen->generic_instances[i].mangled_name);
    }
    emit(gen, "\n");

    // Emit enum typedefs (after struct forward declarations)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue; // Skip generic enum templates

            const char* ename       = decl->as.enum_decl.name;
            int         value_count = decl->as.enum_decl.values.count;

            // Determine if this is a data enum (any variant has types)
            int has_data = 0;
            for (int v = 0; v < value_count; v++) {
                Node* var = decl->as.enum_decl.values.nodes[v];
                if (var->as.enum_variant.types.count > 0) {
                    has_data = 1;
                    break;
                }
            }

            if (!has_data) {
                // Simple enum: typedef enum Name { ... } Name;
                emit(gen, "typedef enum %s {\n", ename);
                gen->indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var = decl->as.enum_decl.values.nodes[v];
                    emit_indent(gen);
                    emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                    if (v < value_count - 1)
                        emit(gen, ",");
                    emit(gen, "\n");
                }
                gen->indent--;
                emit(gen, "} %s;\n\n", ename);
            } else {
                // Data enum: tag enum + tagged union struct
                // 1. Tag enum
                emit(gen, "typedef enum %s_Tag {\n", ename);
                gen->indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var = decl->as.enum_decl.values.nodes[v];
                    emit_indent(gen);
                    emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                    if (v < value_count - 1)
                        emit(gen, ",");
                    emit(gen, "\n");
                }
                gen->indent--;
                emit(gen, "} %s_Tag;\n\n", ename);

                // 2. Tagged union struct
                emit(gen, "typedef struct %s {\n", ename);
                gen->indent++;
                emit_indent(gen);
                emit(gen, "%s_Tag tag;\n", ename);
                emit_indent(gen);
                emit(gen, "union {\n");
                gen->indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var        = decl->as.enum_decl.values.nodes[v];
                    int   type_count = var->as.enum_variant.types.count;
                    if (type_count == 0)
                        continue;
                    emit_indent(gen);
                    emit(gen, "struct {");
                    for (int t = 0; t < type_count; t++) {
                        emit(gen, " ");
                        emit_type(gen, var->as.enum_variant.types.nodes[t]);
                        emit(gen, " f%d;", t);
                    }
                    emit(gen, " } %.*s;\n", var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                }
                gen->indent--;
                emit_indent(gen);
                emit(gen, "};\n");
                gen->indent--;
                emit(gen, "} %s;\n\n", ename);
            }
        }
    }

    // Emit generic enum instance typedefs
    for (int gi = 0; gi < gen->generic_instance_count; gi++) {
        GenericInstance* info = &gen->generic_instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        Node* template = find_generic_enum_decl(ast, info->base_name);
        if (!template)
            continue;

        const char* ename       = info->mangled_name;
        int         value_count = template->as.enum_decl.values.count;

        // Build substitution context
        int              param_count = template->as.enum_decl.type_param_count;
        TypeSubstContext subst;
        subst.type_params = template->as.enum_decl.type_params;
        subst.type_args   = info->type_args;
        subst.count       = param_count;

        TypeSubstContext* old_subst = gen->subst_ctx;
        gen->subst_ctx              = &subst;

        // Determine if this is a data enum
        int has_data = 0;
        for (int v = 0; v < value_count; v++) {
            Node* var = template->as.enum_decl.values.nodes[v];
            if (var->as.enum_variant.types.count > 0) {
                has_data = 1;
                break;
            }
        }

        if (!has_data) {
            // Simple enum
            emit(gen, "typedef enum %s {\n", ename);
            gen->indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var = template->as.enum_decl.values.nodes[v];
                emit_indent(gen);
                emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                if (v < value_count - 1)
                    emit(gen, ",");
                emit(gen, "\n");
            }
            gen->indent--;
            emit(gen, "} %s;\n\n", ename);
        } else {
            // Data enum: tag enum + tagged union struct
            emit(gen, "typedef enum %s_Tag {\n", ename);
            gen->indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var = template->as.enum_decl.values.nodes[v];
                emit_indent(gen);
                emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                if (v < value_count - 1)
                    emit(gen, ",");
                emit(gen, "\n");
            }
            gen->indent--;
            emit(gen, "} %s_Tag;\n\n", ename);

            emit(gen, "typedef struct %s {\n", ename);
            gen->indent++;
            emit_indent(gen);
            emit(gen, "%s_Tag tag;\n", ename);
            emit_indent(gen);
            emit(gen, "union {\n");
            gen->indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var        = template->as.enum_decl.values.nodes[v];
                int   type_count = var->as.enum_variant.types.count;
                if (type_count == 0)
                    continue;
                emit_indent(gen);
                emit(gen, "struct {");
                for (int t = 0; t < type_count; t++) {
                    emit(gen, " ");
                    emit_type(gen, var->as.enum_variant.types.nodes[t]);
                    emit(gen, " f%d;", t);
                }
                emit(gen, " } %.*s;\n", var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
            }
            gen->indent--;
            emit_indent(gen);
            emit(gen, "};\n");
            gen->indent--;
            emit(gen, "} %s;\n\n", ename);
        }

        gen->subst_ctx = old_subst;
    }

    // Emit enum RC helper declarations and definitions
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue; // Skip generic templates
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_rc_fields(gen, ename))
                continue;
            emit(gen, "static inline void __rc_inc_%s(%s v);\n", ename, ename);
            emit(gen, "static inline void __rc_dec_%s(%s v);\n", ename, ename);
        }
    }
    // Forward declarations for generic enum RC helpers
    for (int gi = 0; gi < gen->generic_instance_count; gi++) {
        GenericInstance* info = &gen->generic_instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_rc_fields(gen, info->mangled_name))
            continue;
        emit(gen, "static inline void __rc_inc_%s(%s v);\n", info->mangled_name,
             info->mangled_name);
        emit(gen, "static inline void __rc_dec_%s(%s v);\n", info->mangled_name,
             info->mangled_name);
    }
    if (gen->enum_name_count > 0)
        emit(gen, "\n");

    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue; // Skip generic templates
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_rc_fields(gen, ename))
                continue;

            emit(gen, "static inline void __rc_inc_%s(%s v) {\n", ename, ename);
            emit(gen, "    switch (v.tag) {\n");
            for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
                Node* var = decl->as.enum_decl.values.nodes[v];
                if (var->as.enum_variant.types.count == 0)
                    continue;
                emit(gen, "    case %s_%.*s:\n", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                    Node* tnode = var->as.enum_variant.types.nodes[t];
                    if (!type_node_has_rc(gen, tnode))
                        continue;
                    if (tnode->type == NODE_IDENT && is_enum_type_name(gen, tnode->as.ident.name)) {
                        emit(gen, "        __rc_inc_%s(v.%.*s.f%d);\n", tnode->as.ident.name,
                             var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                    } else {
                        emit(gen, "        __rc_inc(v.%.*s.f%d);\n",
                             var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                    }
                }
                emit(gen, "        break;\n");
            }
            emit(gen, "    default: break;\n");
            emit(gen, "    }\n");
            emit(gen, "}\n\n");

            emit(gen, "static inline void __rc_dec_%s(%s v) {\n", ename, ename);
            emit(gen, "    switch (v.tag) {\n");
            for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
                Node* var = decl->as.enum_decl.values.nodes[v];
                if (var->as.enum_variant.types.count == 0)
                    continue;
                emit(gen, "    case %s_%.*s:\n", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                    Node* tnode = var->as.enum_variant.types.nodes[t];
                    if (!type_node_has_rc(gen, tnode))
                        continue;
                    if (tnode->type == NODE_IDENT && is_enum_type_name(gen, tnode->as.ident.name)) {
                        emit(gen, "        __rc_dec_%s(v.%.*s.f%d);\n", tnode->as.ident.name,
                             var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                    } else {
                        emit(gen, "        __rc_dec(v.%.*s.f%d);\n",
                             var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                    }
                }
                emit(gen, "        break;\n");
            }
            emit(gen, "    default: break;\n");
            emit(gen, "    }\n");
            emit(gen, "}\n\n");
        }
    }

    // Emit RC helpers for generic enum instances
    for (int gi = 0; gi < gen->generic_instance_count; gi++) {
        GenericInstance* info = &gen->generic_instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_rc_fields(gen, info->mangled_name))
            continue;
        Node* tmpl = find_generic_enum_decl(ast, info->base_name);
        if (!tmpl)
            continue;

        const char* ename = info->mangled_name;

        // Set up substitution context
        int              param_count = tmpl->as.enum_decl.type_param_count;
        TypeSubstContext subst;
        subst.type_params = tmpl->as.enum_decl.type_params;
        subst.type_args   = info->type_args;
        subst.count       = param_count;

        TypeSubstContext* old_subst = gen->subst_ctx;
        gen->subst_ctx              = &subst;

        // __rc_inc
        emit(gen, "static inline void __rc_inc_%s(%s v) {\n", ename, ename);
        emit(gen, "    switch (v.tag) {\n");
        for (int v = 0; v < tmpl->as.enum_decl.values.count; v++) {
            Node* var = tmpl->as.enum_decl.values.nodes[v];
            if (var->as.enum_variant.types.count == 0)
                continue;
            emit(gen, "    case %s_%.*s:\n", ename, var->as.enum_variant.name_length,
                 var->as.enum_variant.name);
            for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                Node* tnode = var->as.enum_variant.types.nodes[t];
                if (!type_node_has_rc(gen, tnode))
                    continue;
                const char* enum_nm = resolve_enum_name(gen, tnode);
                if (enum_nm) {
                    emit(gen, "        __rc_inc_%s(v.%.*s.f%d);\n", enum_nm,
                         var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                } else {
                    emit(gen, "        __rc_inc(v.%.*s.f%d);\n", var->as.enum_variant.name_length,
                         var->as.enum_variant.name, t);
                }
            }
            emit(gen, "        break;\n");
        }
        emit(gen, "    default: break;\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");

        // __rc_dec
        emit(gen, "static inline void __rc_dec_%s(%s v) {\n", ename, ename);
        emit(gen, "    switch (v.tag) {\n");
        for (int v = 0; v < tmpl->as.enum_decl.values.count; v++) {
            Node* var = tmpl->as.enum_decl.values.nodes[v];
            if (var->as.enum_variant.types.count == 0)
                continue;
            emit(gen, "    case %s_%.*s:\n", ename, var->as.enum_variant.name_length,
                 var->as.enum_variant.name);
            for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                Node* tnode = var->as.enum_variant.types.nodes[t];
                if (!type_node_has_rc(gen, tnode))
                    continue;
                const char* enum_nm = resolve_enum_name(gen, tnode);
                if (enum_nm) {
                    emit(gen, "        __rc_dec_%s(v.%.*s.f%d);\n", enum_nm,
                         var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                } else {
                    emit(gen, "        __rc_dec(v.%.*s.f%d);\n", var->as.enum_variant.name_length,
                         var->as.enum_variant.name, t);
                }
            }
            emit(gen, "        break;\n");
        }
        emit(gen, "    default: break;\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");

        gen->subst_ctx = old_subst;
    }

    // Emit body typedefs for non-generic structs (must come before __rc_dec_TypeName)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL) {
                if (decl->as.struct_decl.type_param_count > 0)
                    continue;
                emit(gen, "typedef struct %s {\n", decl->as.struct_decl.name);
                gen->indent++;
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    emit_indent(gen);
                    emit_type_with_name(gen, field->as.field.type, field->as.field.name);
                    emit(gen, ";\n");
                }
                gen->indent--;
                emit(gen, "} %s;\n\n", decl->as.struct_decl.name);
            }
        }
    }

    // Emit typedefs for instantiated generic structs
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* info     = &gen->generic_instances[i];
        Node*            template = find_generic_struct_decl(ast, info->base_name);
        if (!template) {
            continue;
        }
        emit(gen, "typedef struct %s {\n", info->mangled_name);
        gen->indent++;

        // Build type substitution map
        int param_count = template->as.struct_decl.type_param_count;
        // Note: info->type_arg_count should equal param_count

        // Emit fields with substituted types
        for (int f = 0; f < template->as.struct_decl.fields.count; f++) {
            Node* field = template->as.struct_decl.fields.nodes[f];
            emit_indent(gen);

            // Check if field type is a type parameter
            if (field->as.field.type && field->as.field.type->type == NODE_IDENT) {
                const char* type_name = field->as.field.type->as.ident.name;
                int         found     = 0;
                for (int p = 0; p < param_count; p++) {
                    if (strcmp(template->as.struct_decl.type_params[p], type_name) == 0) {
                        // Substitute with actual type
                        emit_resolved_type(gen, info->type_args[p]);
                        found = 1;
                        break;
                    }
                }
                if (!found) {
                    emit_type(gen, field->as.field.type);
                }
            } else {
                emit_type(gen, field->as.field.type);
            }

            emit(gen, " %s;\n", field->as.field.name);
        }

        gen->indent--;
        emit(gen, "} %s;\n\n", info->mangled_name);
    }

    // Forward declarations for functions and methods
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Determine if this is a library module (not "main")
        const char* module_prefix = NULL;
        if (strcmp(mod->as.module.name, "main") != 0) {
            module_prefix = mod->as.module.name;
        }
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];

            // Collect func_decl nodes: either top-level or inside impl blocks
            Node** funcs      = NULL;
            int    func_count = 0;
            int    func_cap   = 0;

            if (decl->type == NODE_FUNC_DECL) {
                VEC_GROW(funcs, func_count, func_cap);
                funcs[func_count++] = decl;
            } else if (decl->type == NODE_IMPL_DECL) {
                for (int k = 0; k < decl->as.impl_decl.methods.count; k++) {
                    VEC_GROW(funcs, func_count, func_cap);
                    funcs[func_count++] = decl->as.impl_decl.methods.nodes[k];
                }
            }

            for (int fi = 0; fi < func_count; fi++) {
                Node*           fdecl     = funcs[fi];
                func_decl_node* fdn       = &fdecl->as.func_decl;
                int             is_method = (fdn->receiver_type != NULL);

                // Skip generic method templates (they get instantiated separately)
                if (is_method && fdn->receiver_type_args.count > 0) {
                    continue;
                }

                // Emit static for private functions (except main)
                if (!fdn->is_public && strcmp(fdn->name, "main") != 0) {
                    emit(gen, "static ");
                }

                emit_type(gen, fdn->return_type);

                // Emit function name with appropriate prefix
                emit_function_name(gen, fdn->name, is_method ? fdn->receiver_type : NULL,
                                   module_prefix);

                // Emit self parameter for methods
                if (is_method) {
                    if (fdn->receiver_is_const) {
                        emit(gen, "const ");
                    }
                    emit(gen, "%s* self", fdn->receiver_type);
                    if (fdn->params.count > 0) {
                        emit(gen, ", ");
                    }
                }

                if (fdn->params.count == 0 && !is_method) {
                    emit(gen, "void");
                } else {
                    for (int j = 0; j < fdn->params.count; j++) {
                        if (j > 0)
                            emit(gen, ", ");
                        Node* param = fdn->params.nodes[j];
                        emit_type_with_name(gen, param->as.param.type, param->as.param.name);
                    }
                }
                emit(gen, ");\n");
            }
            free(funcs);
        }
    }

    // Forward declarations for instantiated generic methods
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* info = &gen->generic_instances[i];

        // Find the generic struct template to get type params
        Node* template = find_generic_struct_decl(ast, info->base_name);
        if (!template)
            continue;

        // Find all methods for this generic struct
        Node** methods      = NULL;
        int    method_count = 0;
        collect_generic_methods(ast, info->base_name, &methods, &method_count);

        // Emit forward declaration for each method
        for (int j = 0; j < method_count; j++) {
            Node*           method = methods[j];
            func_decl_node* fdn    = &method->as.func_decl;

            // Extract method-specific type bindings
            char** method_params     = NULL;
            Type** method_args       = NULL;
            int    method_bind_count = 0;
            codegen_extract_method_bindings(&fdn->receiver_type_args, info->type_args,
                                            info->type_arg_count, &method_params, &method_args,
                                            &method_bind_count);

            // Build combined substitution context
            int    combined_count  = template->as.struct_decl.type_param_count + method_bind_count;
            char** combined_params = xmalloc(combined_count * sizeof(char*));
            Type** combined_args   = xmalloc(combined_count * sizeof(Type*));

            for (int k = 0; k < template->as.struct_decl.type_param_count; k++) {
                combined_params[k] = template->as.struct_decl.type_params[k];
                combined_args[k]   = info->type_args[k];
            }
            for (int k = 0; k < method_bind_count; k++) {
                combined_params[template->as.struct_decl.type_param_count + k] = method_params[k];
                combined_args[template->as.struct_decl.type_param_count + k]   = method_args[k];
            }

            TypeSubstContext subst_ctx;
            subst_ctx.type_params = combined_params;
            subst_ctx.type_args   = combined_args;
            subst_ctx.count       = combined_count;
            gen->subst_ctx        = &subst_ctx;

            // Return type (with substitution)
            emit_type(gen, fdn->return_type);

            // Method name: MangledStruct_methodname(
            emit(gen, " %s_%s(", info->mangled_name, fdn->name);

            // Self parameter
            if (fdn->receiver_is_const) {
                emit(gen, "const ");
            }
            emit(gen, "%s* self", info->mangled_name);

            // Other parameters
            for (int p = 0; p < fdn->params.count; p++) {
                emit(gen, ", ");
                Node* param = fdn->params.nodes[p];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
            emit(gen, ");\n");

            gen->subst_ctx = NULL;
            free(combined_params);
            free(combined_args);
            for (int k = 0; k < method_bind_count; k++) {
                free(method_params[k]);
            }
            free(method_params);
            free(method_args);
        }

        free(methods);
    }
    emit(gen, "\n");

    // Forward declare all __rc_dec_TypeName functions to handle cross-references
    // (e.g., non-generic Container referencing __rc_dec_Box_Inner from generic instance)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL)
                continue;
            if (decl->as.struct_decl.type_param_count > 0)
                continue;
            const char* sname = decl->as.struct_decl.name;
            // Check if this struct needs a custom dec function
            int needs_dec = 0;
            for (int t = 0; t < gen->trait_impl_count; t++) {
                if (strcmp(gen->trait_impls[t].trait_name, "Drop") == 0 &&
                    strcmp(gen->trait_impls[t].type_name, sname) == 0) {
                    needs_dec = 1;
                    break;
                }
            }
            if (!needs_dec) {
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    if (field->as.field.type && type_node_has_rc(gen, field->as.field.type)) {
                        needs_dec = 1;
                        break;
                    }
                }
            }
            if (needs_dec) {
                emit(gen, "static inline void __rc_dec_%s(%s* ptr);\n", sname, sname);
            }
        }
    }
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* info = &gen->generic_instances[i];
        Type*            t    = info->type;
        if (!t || t->kind != TYPE_STRUCT)
            continue;
        int has_drop = 0;
        for (int ti = 0; ti < gen->trait_impl_count; ti++) {
            if (strcmp(gen->trait_impls[ti].trait_name, "Drop") == 0 &&
                strcmp(gen->trait_impls[ti].type_name, info->base_name) == 0) {
                has_drop = 1;
                break;
            }
        }
        if (has_drop || t->as.struc.has_rc_fields) {
            emit(gen, "static inline void __rc_dec_%s(%s* ptr);\n", info->mangled_name,
                 info->mangled_name);
        }
    }
    emit(gen, "\n");

    // Emit type-specific __rc_dec_TypeName functions for structs with Drop or RC fields
    // Non-generic structs: scan modules for struct decls with resolved types
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL)
                continue;
            if (decl->as.struct_decl.type_param_count > 0)
                continue; // Skip generic templates

            // Look up the resolved type from the checker's symbol table
            // We stored it when defining the struct - find it via generic_instances or
            // re-derive from the AST decl. Since we don't have direct access to the checker's
            // symbol table from codegen, we use the struct name to find the type in
            // generic_instances. For non-generic structs we need to find the Type*.
            // However, we can scan the AST and look at the struct decl's type info.
            // The actual Type* with flags is in checker scope, not directly available here.
            // We need to find the type through the struct fields in the emitted forward decls.
            // Alternative: iterate trait_impls to check has_drop, and scan field types for
            // has_rc_fields by looking at field type names.

            const char* sname = decl->as.struct_decl.name;

            // Check if this struct implements Drop
            int has_drop = 0;
            for (int t = 0; t < gen->trait_impl_count; t++) {
                if (strcmp(gen->trait_impls[t].trait_name, "Drop") == 0 &&
                    strcmp(gen->trait_impls[t].type_name, sname) == 0) {
                    has_drop = 1;
                    break;
                }
            }

            // Check if any field carries RC-managed values
            int has_rc_fields = 0;
            // Collect RC field info for emission
            int    rc_field_count      = 0;
            char** rc_field_names      = NULL;
            char** rc_field_type_names = NULL;
            int*   rc_field_is_enum    = NULL;
            for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                Node* field = decl->as.struct_decl.fields.nodes[f];
                if (field->as.field.type && type_node_has_rc(gen, field->as.field.type)) {
                    has_rc_fields = 1;
                    rc_field_count++;
                    rc_field_names = xrealloc(rc_field_names, rc_field_count * sizeof(char*));
                    rc_field_type_names =
                        xrealloc(rc_field_type_names, rc_field_count * sizeof(char*));
                    rc_field_is_enum = xrealloc(rc_field_is_enum, rc_field_count * sizeof(int));
                    rc_field_names[rc_field_count - 1] = xstrdup(field->as.field.name);
                    // Get the type name for the field
                    if (field->as.field.type->type == NODE_IDENT) {
                        rc_field_type_names[rc_field_count - 1] =
                            xstrdup(field->as.field.type->as.ident.name);
                        rc_field_is_enum[rc_field_count - 1] =
                            is_enum_type_name(gen, field->as.field.type->as.ident.name);
                    } else if (field->as.field.type->type == NODE_GENERIC_TYPE) {
                        // Build mangled name for generic type field (e.g., Box<Inner> -> Box_Inner)
                        Node*  gtype     = field->as.field.type;
                        int    arg_count = gtype->as.generic_type.type_args.count;
                        Type** args      = xmalloc(arg_count * sizeof(Type*));
                        for (int a = 0; a < arg_count; a++) {
                            args[a] = type_from_node(gtype->as.generic_type.type_args.nodes[a]);
                        }
                        rc_field_type_names[rc_field_count - 1] =
                            type_mangle_generic(gtype->as.generic_type.base_name, args, arg_count);
                        free(args);
                        rc_field_is_enum[rc_field_count - 1] = 0;
                    } else {
                        rc_field_type_names[rc_field_count - 1] = NULL;
                        rc_field_is_enum[rc_field_count - 1]    = 0;
                    }
                }
            }

            if (!has_drop && !has_rc_fields)
                continue;

            // Emit: static inline void __rc_dec_TypeName(TypeName* ptr) { ... }
            emit(gen, "static inline void __rc_dec_%s(%s* ptr) {\n", sname, sname);
            emit(gen, "    if (!ptr) return;\n");
            emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
            emit(gen, "    if (--h->refcount == 0) {\n");
            if (has_drop) {
                emit(gen, "        %s_drop(ptr);\n", sname);
            }
            for (int f = 0; f < rc_field_count; f++) {
                const char* field_tname = rc_field_type_names[f];
                if (rc_field_is_enum[f]) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", field_tname, rc_field_names[f]);
                } else {
                    // Check if the field's type itself has a type-specific dec function
                    int field_needs_custom_dec = 0;
                    if (field_tname) {
                        // Check if field type implements Drop (exact name match for non-generic)
                        for (int t = 0; t < gen->trait_impl_count; t++) {
                            if (strcmp(gen->trait_impls[t].trait_name, "Drop") == 0 &&
                                strcmp(gen->trait_impls[t].type_name, field_tname) == 0) {
                                field_needs_custom_dec = 1;
                                break;
                            }
                        }
                        // Check if field type has RC fields (scan non-generic structs)
                        if (!field_needs_custom_dec) {
                            for (int si = 0;
                                 si < ast->as.program.modules.count && !field_needs_custom_dec;
                                 si++) {
                                Node* smod = ast->as.program.modules.nodes[si];
                                if (!smod || smod->type != NODE_MODULE)
                                    continue;
                                for (int sj = 0; sj < smod->as.module.decls.count; sj++) {
                                    Node* sdecl = smod->as.module.decls.nodes[sj];
                                    if (sdecl->type == NODE_STRUCT_DECL &&
                                        sdecl->as.struct_decl.type_param_count == 0 &&
                                        strcmp(sdecl->as.struct_decl.name, field_tname) == 0) {
                                        for (int sf = 0; sf < sdecl->as.struct_decl.fields.count;
                                             sf++) {
                                            Node* sfield = sdecl->as.struct_decl.fields.nodes[sf];
                                            if (sfield->as.field.type &&
                                                type_node_has_rc(gen, sfield->as.field.type)) {
                                                field_needs_custom_dec = 1;
                                                break;
                                            }
                                        }
                                        break;
                                    }
                                }
                            }
                        }
                        // Also check generic instances (e.g., field type "Box_Inner")
                        if (!field_needs_custom_dec) {
                            for (int gi = 0; gi < gen->generic_instance_count; gi++) {
                                if (strcmp(gen->generic_instances[gi].mangled_name, field_tname) ==
                                    0) {
                                    Type* git = gen->generic_instances[gi].type;
                                    if (git && git->kind == TYPE_STRUCT &&
                                        (git->as.struc.has_drop || git->as.struc.has_rc_fields)) {
                                        field_needs_custom_dec = 1;
                                    }
                                    break;
                                }
                            }
                        }
                    }
                    if (field_needs_custom_dec) {
                        emit(gen, "        __rc_dec_%s(ptr->%s);\n", field_tname,
                             rc_field_names[f]);
                    } else {
                        emit(gen, "        __rc_dec(ptr->%s);\n", rc_field_names[f]);
                    }
                }
            }
            if (gen->rc_debug) {
                emit(gen, "        fprintf(stderr, \"RC_FREE: %%p\\n\", (void*)ptr);\n");
            }
            emit(gen, "        free(h);\n");
            if (gen->rc_debug) {
                emit(gen, "    } else {\n");
                emit(gen, "        fprintf(stderr, \"RC_DEC: %%p (rc=%%zu)\\n\", (void*)ptr, "
                          "h->refcount);\n");
            }
            emit(gen, "    }\n");
            emit(gen, "}\n\n");

            for (int f = 0; f < rc_field_count; f++) {
                free(rc_field_names[f]);
                free(rc_field_type_names[f]);
            }
            free(rc_field_names);
            free(rc_field_type_names);
        }
    }

    // Generic instances: emit __rc_dec_MangledName for those with Drop or RC fields
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* info = &gen->generic_instances[i];
        Type*            t    = info->type;
        if (!t || t->kind != TYPE_STRUCT)
            continue;

        // Check if this generic type implements Drop
        int has_drop = 0;
        for (int ti = 0; ti < gen->trait_impl_count; ti++) {
            if (strcmp(gen->trait_impls[ti].trait_name, "Drop") == 0 &&
                strcmp(gen->trait_impls[ti].type_name, info->base_name) == 0) {
                has_drop = 1;
                break;
            }
        }

        int has_rc_fields = t->as.struc.has_rc_fields;

        if (!has_drop && !has_rc_fields)
            continue;

        const char* mname = info->mangled_name;
        emit(gen, "static inline void __rc_dec_%s(%s* ptr) {\n", mname, mname);
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
        emit(gen, "    if (--h->refcount == 0) {\n");
        if (has_drop) {
            emit(gen, "        %s_drop(ptr);\n", mname);
        }
        for (int f = 0; f < t->as.struc.field_count; f++) {
            Type* ft = t->as.struc.field_types[f];
            if (ft && ft->kind == TYPE_STRUCT) {
                if (ft->as.struc.has_drop || ft->as.struc.has_rc_fields) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", ft->as.struc.name,
                         t->as.struc.field_names[f]);
                } else {
                    emit(gen, "        __rc_dec(ptr->%s);\n", t->as.struc.field_names[f]);
                }
            }
        }
        if (gen->rc_debug) {
            emit(gen, "        fprintf(stderr, \"RC_FREE: %%p\\n\", (void*)ptr);\n");
        }
        emit(gen, "        free(h);\n");
        if (gen->rc_debug) {
            emit(gen, "    } else {\n");
            emit(gen, "        fprintf(stderr, \"RC_DEC: %%p (rc=%%zu)\\n\", (void*)ptr, "
                      "h->refcount);\n");
        }
        emit(gen, "    }\n");
        emit(gen, "}\n\n");
    }

    // Emit all declarations
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Set current module context (NULL for "main", module name for library imports)
        gen->current_module = strcmp(mod->as.module.name, "main") == 0 ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            emit_decl(gen, mod->as.module.decls.nodes[i]);
        }
    }
    gen->current_module = NULL;

    // Emit implementations for instantiated generic methods
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* info = &gen->generic_instances[i];

        // Find the generic struct template to get type params
        Node* template = find_generic_struct_decl(ast, info->base_name);
        if (!template)
            continue;

        // Find all methods for this generic struct
        Node** methods      = NULL;
        int    method_count = 0;
        collect_generic_methods(ast, info->base_name, &methods, &method_count);

        // Emit implementation for each method
        for (int j = 0; j < method_count; j++) {
            Node*           method = methods[j];
            func_decl_node* fdn    = &method->as.func_decl;

            // Extract method-specific type bindings
            char** method_params     = NULL;
            Type** method_args       = NULL;
            int    method_bind_count = 0;
            codegen_extract_method_bindings(&fdn->receiver_type_args, info->type_args,
                                            info->type_arg_count, &method_params, &method_args,
                                            &method_bind_count);

            // Build combined substitution context
            int    combined_count  = template->as.struct_decl.type_param_count + method_bind_count;
            char** combined_params = xmalloc(combined_count * sizeof(char*));
            Type** combined_args   = xmalloc(combined_count * sizeof(Type*));

            for (int k = 0; k < template->as.struct_decl.type_param_count; k++) {
                combined_params[k] = template->as.struct_decl.type_params[k];
                combined_args[k]   = info->type_args[k];
            }
            for (int k = 0; k < method_bind_count; k++) {
                combined_params[template->as.struct_decl.type_param_count + k] = method_params[k];
                combined_args[template->as.struct_decl.type_param_count + k]   = method_args[k];
            }

            TypeSubstContext subst_ctx;
            subst_ctx.type_params = combined_params;
            subst_ctx.type_args   = combined_args;
            subst_ctx.count       = combined_count;
            gen->subst_ctx        = &subst_ctx;

            // Check if function is void
            int is_void =
                !fdn->return_type || (fdn->return_type->type == NODE_IDENT &&
                                      strcmp(fdn->return_type->as.ident.name, "void") == 0);

            // Return type (with substitution)
            emit_type(gen, fdn->return_type);

            // Method name: MangledStruct_methodname(
            emit(gen, " %s_%s(", info->mangled_name, fdn->name);

            // Self parameter
            if (fdn->receiver_is_const) {
                emit(gen, "const ");
            }
            emit(gen, "%s* self", info->mangled_name);

            // Other parameters
            for (int p = 0; p < fdn->params.count; p++) {
                emit(gen, ", ");
                Node* param = fdn->params.nodes[p];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
            emit(gen, ") {\n");

            // Clear defer stack for this function
            defer_clear(gen);
            gen->current_return_type = fdn->return_type;

            // First pass: count defers to know if we need __ret
            int has_defers = 0;
            if (fdn->body) {
                for (int s = 0; s < fdn->body->as.block.stmts.count; s++) {
                    Node* stmt = fdn->body->as.block.stmts.nodes[s];
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
                emit_type(gen, fdn->return_type);
                emit(gen, " __ret;\n");
            }

            // Emit function body
            if (fdn->body) {
                for (int s = 0; s < fdn->body->as.block.stmts.count; s++) {
                    emit_stmt(gen, fdn->body->as.block.stmts.nodes[s]);
                }
            }

            // Emit any remaining defers at function end (for void functions or fallthrough)
            if (has_defers) {
                for (int d = gen->defer_count - 1; d >= 0; d--) {
                    emit_stmt(gen, gen->defer_stack[d]);
                }
            }

            gen->indent--;
            emit(gen, "}\n\n");

            // Clear RC tracking for generic method
            rc_clear_all(gen);

            gen->subst_ctx = NULL;
            free(combined_params);
            free(combined_args);
            for (int k = 0; k < method_bind_count; k++) {
                free(method_params[k]);
            }
            free(method_params);
            free(method_args);
        }

        free(methods);
    }
}
