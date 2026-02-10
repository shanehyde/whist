#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_internal.h"
#include "types.h"
#include "vec.h"

// Emit indentation spaces (4 per level) at the current indent depth
void emit_indent(CodeGen* gen) {
    for (int i = 0; i < gen->indent; i++) {
        fprintf(gen->out, "    ");
    }
}

// Emit formatted text to the output stream
void emit(CodeGen* gen, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out, fmt, args);
    va_end(args);
}

// Push a deferred statement onto the defer stack for later LIFO emission
void defer_push(CodeGen* gen, Node* node) {
    VEC_GROW(gen->defer_stack, gen->defer_count, gen->defer_capacity);
    gen->defer_stack[gen->defer_count++] = node;
}

// Reset the defer stack (called at function boundaries)
void defer_clear(CodeGen* gen) {
    gen->defer_count = 0;
}

// Check if a name is a registered enum type
// Check if a name is a registered enum type in the codegen context
int is_enum_type_name(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enum_name_count; i++) {
        if (strcmp(gen->enum_names[i], name) == 0)
            return 1;
    }
    return 0;
}

// Return the index of an enum name in the registered enum list, or -1 if not found
int enum_index(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enum_name_count; i++) {
        if (strcmp(gen->enum_names[i], name) == 0)
            return i;
    }
    return -1;
}

// Check if a named enum type has RC-managed fields
int enum_has_rc_fields(CodeGen* gen, const char* name) {
    int idx = enum_index(gen, name);
    if (idx < 0 || !gen->enum_has_rc_fields)
        return 0;
    return gen->enum_has_rc_fields[idx];
}

// Resolve a type node through aliases. If the node is a NODE_IDENT
// that names a type alias, return the alias target node instead.
Node* resolve_alias(CodeGen* gen, Node* type_node) {
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
int is_struct_type(CodeGen* gen, Node* type_node) {
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

// Check if a type node represents an RC-managed type (struct, Vec, or enum with RC fields)
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

        if (is_vec || is_span) {
            emit(gen, "%s", is_vec ? "__Vec_" : "__Span_");
            Node* arg = type_node->as.generic_type.type_args.nodes[0];
            if (arg->type == NODE_IDENT) {
                const char* arg_name    = arg->as.ident.name;
                int         substituted = 0;
                if (gen->subst_ctx) {
                    for (int s = 0; s < gen->subst_ctx->count; s++) {
                        if (strcmp(gen->subst_ctx->type_params[s], arg_name) == 0) {
                            Type* subst_type = gen->subst_ctx->type_args[s];
                            emit(gen, "%s", type_mangle_name(subst_type));
                            substituted = 1;
                            break;
                        }
                    }
                }
                if (!substituted) {
                    emit(gen, "%s", arg_name);
                }
            } else if (arg->type == NODE_GENERIC_TYPE) {
                // Nested generic like Vec<HashEntry<V>> — build substituted mangled name
                char* mangled = build_mangled_name_from_generic_node(gen, arg);
                emit(gen, "%s", mangled);
                free(mangled);
            }
            if (is_vec)
                emit(gen, "*"); // Vec is RC-managed pointer
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
        emit(gen, "__Span_%s", type_mangle_name(type->as.span.elem));
        break;
    case TYPE_VEC:
        emit(gen, "__Vec_%s*", type_mangle_name(type->as.vec.elem));
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

// Map a binary operator token to its C operator string
const char* binary_op_str(TokenType op) {
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

// Map a unary operator token to its C operator string
const char* unary_op_str(TokenType op) {
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

// Map an assignment operator token to its C operator string
const char* assign_op_str(TokenType op) {
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

// Emit an extern module: #include directive and register function aliases/names
static void emit_extern_module(CodeGen* gen, Node* node) {
    emit(gen, "\n#include <%s.h>\n", node->as.extern_module.module_name);
    // Register extern function aliases and names
    for (int i = 0; i < node->as.extern_module.decls.count; i++) {
        Node* decl = node->as.extern_module.decls.nodes[i];
        if (decl->type == NODE_FUNC_DECL) {
            // Track all extern function names (for intra-module call prefixing)
            VEC_GROW(gen->extern_funcs, gen->extern_func_count, gen->extern_func_capacity);
            gen->extern_funcs[gen->extern_func_count++] = decl->as.func_decl.name;
            // Register aliases (Whist name -> C name) for renamed externs
            if (decl->as.func_decl.extern_name) {
                VEC_GROW(gen->extern_aliases, gen->extern_alias_count, gen->extern_alias_capacity);
                gen->extern_aliases[gen->extern_alias_count].whist_name = decl->as.func_decl.name;
                gen->extern_aliases[gen->extern_alias_count].c_name =
                    decl->as.func_decl.extern_name;
                gen->extern_alias_count++;
            }
        }
    }
}

// Emit a function declaration: signature, body, defer cleanup, and RC cleanup
static void emit_func_decl(CodeGen* gen, Node* node) {
    int is_method = (node->as.func_decl.receiver_type != NULL);

    // Skip generic method templates - they get instantiated separately
    if (is_method && node->as.func_decl.receiver_type_args.count > 0) {
        return;
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
                       is_method ? node->as.func_decl.receiver_type : NULL, gen->current_module);

    // Parameters
    if (is_method) {
        // Emit self parameter first
        if (node->as.func_decl.receiver_is_const &&
            strcmp(node->as.func_decl.receiver_type, "string") != 0) {
            emit(gen, "const ");
        }
        if (type_is_builtin_name(node->as.func_decl.receiver_type)) {
            emit(gen, "%s self", type_c_name(node->as.func_decl.receiver_type));
        } else {
            emit(gen, "%s* self", node->as.func_decl.receiver_type);
        }
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
}

// Emit a top-level declaration: function, struct, enum, extern, impl, or global var
void emit_decl(CodeGen* gen, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXTERN_MODULE:
        emit_extern_module(gen, node);
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

    case NODE_USE_DECL:
        // Register use aliases for function calls (types don't need aliases since
        // struct/enum names in generated C are bare, not module-prefixed)
        for (int i = 0; i < node->as.use_decl.symbol_count; i++) {
            char* sym_name = node->as.use_decl.symbol_names[i];
            char* mod_name = node->as.use_decl.module_name;

            // Build the C function name: module_symbolname
            // Special case: std.format -> __std_format (compiler builtin)
            char c_name[256];
            if (strcmp(mod_name, "std") == 0 && strcmp(sym_name, "format") == 0) {
                snprintf(c_name, sizeof(c_name), "__std_format");
            } else {
                snprintf(c_name, sizeof(c_name), "%s_%s", mod_name, sym_name);
            }

            VEC_GROW(gen->use_aliases, gen->use_alias_count, gen->use_alias_capacity);
            gen->use_aliases[gen->use_alias_count].whist_name = xstrdup(sym_name);
            gen->use_aliases[gen->use_alias_count].c_name     = xstrdup(c_name);
            gen->use_alias_count++;
        }
        break;

    case NODE_IMPL_DECL:
        // Emit each method in the impl block as a regular function
        for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
            emit_decl(gen, node->as.impl_decl.methods.nodes[i]);
        }
        break;

    case NODE_FUNC_DECL:
        emit_func_decl(gen, node);
        break;

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
