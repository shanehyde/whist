#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_internal.h"
#include "types.h"
#include "vec.h"

// Emit indentation spaces (4 per level) at the current indent depth
void emit_indent(CodeGen* gen) {
    for (int i = 0; i < gen->out.indent; i++) {
        fprintf(gen->out.file, "    ");
    }
}

// Emit formatted text to the output stream
void emit(CodeGen* gen, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vfprintf(gen->out.file, fmt, args);
    va_end(args);
}

// Push a deferred statement onto the defer stack for later LIFO emission
void defer_push(CodeGen* gen, Node* node) {
    VEC_GROW(gen->defer.stack, gen->defer.count, gen->defer.capacity);
    gen->defer.stack[gen->defer.count++] = node;
}

// Reset the defer stack (called at function boundaries)
void defer_clear(CodeGen* gen) {
    gen->defer.count = 0;
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
                for (int k = 0; k < count; k++)
                    free(params[k]);
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
    } else if (module_name == NULL && strcmp(func_name, "main") == 0) {
        // User main is lowered to an internal symbol; wrapper C main is emitted separately.
        emit(gen, " __w0_user_main(");
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
        Type* resolved = subst_lookup(gen, name);
        if (resolved) {
            emit_resolved_type(gen, resolved);
            return;
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
            for (int i = 0; i < gen->aliases.type_count; i++) {
                if (strcmp(gen->aliases.types[i], name) == 0) {
                    emit_type(gen, gen->aliases.type_targets[i]);
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
                const char* arg_name = arg->as.ident.name;
                Type*       resolved = subst_lookup(gen, arg_name);
                if (resolved) {
                    emit(gen, "%s", type_mangle_name(resolved));
                } else {
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
            Node* arg = type_node->as.generic_type.type_args.nodes[i];
            if (arg->type == NODE_IDENT) {
                const char* arg_name = arg->as.ident.name;
                Type*       resolved = subst_lookup(gen, arg_name);
                if (resolved) {
                    emit(gen, "%s", type_name(resolved));
                } else {
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
                        Type*       resolved_n  = subst_lookup(gen, nested_name);
                        if (resolved_n) {
                            emit(gen, "%s", type_name(resolved_n));
                        } else {
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
    case TYPE_STRINGBUILDER:
        emit(gen, "__StringBuilder*");
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
            VEC_GROW(gen->aliases.extern_funcs, gen->aliases.extern_func_count,
                     gen->aliases.extern_func_capacity);
            gen->aliases.extern_funcs[gen->aliases.extern_func_count++] = decl->as.func_decl.name;
            // Register aliases (Whist name -> C name) for renamed externs
            if (decl->as.func_decl.extern_name) {
                VEC_GROW(gen->aliases.externs, gen->aliases.extern_count,
                         gen->aliases.extern_capacity);
                gen->aliases.externs[gen->aliases.extern_count].whist_name =
                    decl->as.func_decl.name;
                gen->aliases.externs[gen->aliases.extern_count].c_name =
                    decl->as.func_decl.extern_name;
                gen->aliases.extern_count++;
            }
        }
    }
}

static int return_type_is_void(Node* return_type) {
    return !return_type ||
           (return_type->type == NODE_IDENT && strcmp(return_type->as.ident.name, "void") == 0);
}

static void emit_func_return_type(CodeGen* gen, func_decl_node* fdn) {
    if (fdn->return_is_const && !return_type_is_void(fdn->return_type)) {
        emit(gen, "const ");
    }
    emit_type(gen, fdn->return_type);
}

// Emit a function declaration: signature, body, defer cleanup, and RC cleanup
static void emit_func_decl(CodeGen* gen, Node* node) {
    int is_method = (node->as.func_decl.receiver_type != NULL);

    // Skip generic method templates - they get instantiated separately
    if (is_method && node->as.func_decl.receiver_type_args.count > 0) {
        return;
    }

    // Check if function is void
    int is_void = return_type_is_void(node->as.func_decl.return_type);

    // Emit static for private functions (except main)
    if (!node->as.func_decl.is_public && strcmp(node->as.func_decl.name, "main") != 0) {
        emit(gen, "static ");
    }

    // Return type
    emit_func_return_type(gen, &node->as.func_decl);

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
    gen->defer.return_type = node->as.func_decl.return_type;

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
    gen->out.indent++;

    // Declare __ret if function has defers and is non-void
    if (has_defers && !is_void) {
        emit_indent(gen);
        emit_type(gen, node->as.func_decl.return_type);
        emit(gen, " __ret;\n");
    }

    // Track if we're inside an enum method body (for match(self) dereference)
    int was_in_enum_method = gen->in_enum_method;
    if (is_method && is_enum_type_name(gen, node->as.func_decl.receiver_type)) {
        gen->in_enum_method = 1;
    }

    if (node->as.func_decl.body) {
        for (int i = 0; i < node->as.func_decl.body->as.block.stmts.count; i++) {
            emit_stmt(gen, node->as.func_decl.body->as.block.stmts.nodes[i]);
        }
    }

    gen->in_enum_method = was_in_enum_method;

    // Emit cleanup section if there are defers
    if (gen->defer.count > 0) {
        emit(gen, "__cleanup:;\n");
        // Emit deferred statements in reverse order (LIFO)
        for (int i = gen->defer.count - 1; i >= 0; i--) {
            emit_stmt(gen, gen->defer.stack[i]);
        }
        // Emit final return
        emit_indent(gen);
        if (is_void) {
            emit(gen, "return;\n");
        } else {
            emit(gen, "return __ret;\n");
        }
    }

    gen->out.indent--;
    emit(gen, "}\n\n");

    // Clear defer stack and RC tracking
    defer_clear(gen);
    rc_clear_all(gen);
    gen->defer.return_type = NULL;
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

            VEC_GROW(gen->aliases.uses, gen->aliases.use_count, gen->aliases.use_capacity);
            gen->aliases.uses[gen->aliases.use_count].whist_name = xstrdup(sym_name);
            gen->aliases.uses[gen->aliases.use_count].c_name     = xstrdup(c_name);
            gen->aliases.use_count++;
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

// =============================================================================
// Typedef Emission — bounds checks, string helpers, typedefs, forward decls
// =============================================================================

// Bounds-checking helpers are now provided by whist_runtime.h.
void emit_bounds_checks(CodeGen* gen) {
    (void)gen;
}

// String method helpers are now provided by whist_runtime.h.
void emit_string_helpers(CodeGen* gen) {
    (void)gen;
}

// Emit C struct typedefs for each collected tuple type (__tuple_tN)
void emit_tuple_typedefs(CodeGen* gen) {
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
}

// Emit forward typedef declarations for all structs (non-generic and instantiated)
void emit_struct_forward_decls(CodeGen* gen, Node* ast) {
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
    for (int i = 0; i < gen->checker.instance_count; i++) {
        if (gen->checker.instances[i].type->kind == TYPE_ENUM)
            continue;
        emit(gen, "typedef struct %s %s;\n", gen->checker.instances[i].mangled_name,
             gen->checker.instances[i].mangled_name);
    }
    emit(gen, "\n");
}

// Emit struct typedefs for each instantiated Span type (__Span_T)
void emit_span_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->checker.span_count; i++) {
        SpanInstance* inst = &gen->checker.spans[i];
        emit(gen, "typedef struct {\n");
        emit(gen, "    ");
        if (inst->elem_type->kind != TYPE_STRING)
            emit(gen, "const ");
        emit_resolved_type(gen, inst->elem_type);
        emit(gen, "* data;\n");
        emit(gen, "    uint64_t count;\n");
        emit(gen, "} __Span_%s;\n\n", type_mangle_name(inst->elem_type));
    }
}

// Emit struct typedefs for each instantiated Vec type (__Vec_T)
void emit_vec_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst       = &gen->checker.vecs[i];
        Type*        elem_type  = inst->elem_type;
        const char*  elem_tname = type_mangle_name(elem_type);

        emit(gen, "typedef struct {\n");
        emit(gen, "    ");
        emit_resolved_type(gen, elem_type);
        emit(gen, "* data;\n");
        emit(gen, "    int64_t count;\n");
        emit(gen, "    int64_t capacity;\n");
        emit(gen, "} __Vec_%s;\n\n", elem_tname);
    }
}

// Emit C enum/tagged-union typedefs for non-generic enum declarations
void emit_enum_typedefs(CodeGen* gen, Node* ast) {
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
                gen->out.indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var = decl->as.enum_decl.values.nodes[v];
                    emit_indent(gen);
                    emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                    if (var->as.enum_variant.has_explicit_value) {
                        emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                    }
                    if (v < value_count - 1)
                        emit(gen, ",");
                    emit(gen, "\n");
                }
                gen->out.indent--;
                emit(gen, "} %s;\n\n", ename);
            } else {
                // Data enum: tag enum + tagged union struct
                // 1. Tag enum
                emit(gen, "typedef enum %s_Tag {\n", ename);
                gen->out.indent++;
                for (int v = 0; v < value_count; v++) {
                    Node* var = decl->as.enum_decl.values.nodes[v];
                    emit_indent(gen);
                    emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                         var->as.enum_variant.name);
                    if (var->as.enum_variant.has_explicit_value) {
                        emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                    }
                    if (v < value_count - 1)
                        emit(gen, ",");
                    emit(gen, "\n");
                }
                gen->out.indent--;
                emit(gen, "} %s_Tag;\n\n", ename);

                // 2. Tagged union struct
                emit(gen, "typedef struct %s {\n", ename);
                gen->out.indent++;
                emit_indent(gen);
                emit(gen, "%s_Tag tag;\n", ename);
                emit_indent(gen);
                emit(gen, "union {\n");
                gen->out.indent++;
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
                gen->out.indent--;
                emit_indent(gen);
                emit(gen, "};\n");
                gen->out.indent--;
                emit(gen, "} %s;\n\n", ename);
            }
        }
    }
}

// Emit C enum/tagged-union typedefs for instantiated generic enum types
void emit_generic_enum_typedefs(CodeGen* gen, Node* ast) {
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
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

        TypeSubstContext* old_subst = gen->generics.subst;
        gen->generics.subst         = &subst;

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
            gen->out.indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var = template->as.enum_decl.values.nodes[v];
                emit_indent(gen);
                emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                if (var->as.enum_variant.has_explicit_value) {
                    emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                }
                if (v < value_count - 1)
                    emit(gen, ",");
                emit(gen, "\n");
            }
            gen->out.indent--;
            emit(gen, "} %s;\n\n", ename);
        } else {
            // Data enum: tag enum + tagged union struct
            emit(gen, "typedef enum %s_Tag {\n", ename);
            gen->out.indent++;
            for (int v = 0; v < value_count; v++) {
                Node* var = template->as.enum_decl.values.nodes[v];
                emit_indent(gen);
                emit(gen, "%s_%.*s", ename, var->as.enum_variant.name_length,
                     var->as.enum_variant.name);
                if (var->as.enum_variant.has_explicit_value) {
                    emit(gen, " = %ld", var->as.enum_variant.explicit_value);
                }
                if (v < value_count - 1)
                    emit(gen, ",");
                emit(gen, "\n");
            }
            gen->out.indent--;
            emit(gen, "} %s_Tag;\n\n", ename);

            emit(gen, "typedef struct %s {\n", ename);
            gen->out.indent++;
            emit_indent(gen);
            emit(gen, "%s_Tag tag;\n", ename);
            emit_indent(gen);
            emit(gen, "union {\n");
            gen->out.indent++;
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
            gen->out.indent--;
            emit_indent(gen);
            emit(gen, "};\n");
            gen->out.indent--;
            emit(gen, "} %s;\n\n", ename);
        }

        gen->generics.subst = old_subst;
    }
}

// Emit struct body typedefs (field definitions) for non-generic and generic structs
void emit_struct_body_typedefs(CodeGen* gen, Node* ast) {
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
                gen->out.indent++;
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    emit_indent(gen);
                    emit_type_with_name(gen, field->as.field.type, field->as.field.name);
                    emit(gen, ";\n");
                }
                gen->out.indent--;
                emit(gen, "} %s;\n\n", decl->as.struct_decl.name);
            }
        }
    }

    // Emit typedefs for instantiated generic structs
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* info     = &gen->checker.instances[i];
        Node*            template = find_generic_struct_decl(ast, info->base_name);
        if (!template) {
            continue;
        }
        emit(gen, "typedef struct %s {\n", info->mangled_name);
        gen->out.indent++;

        // Set up substitution context so emit_type handles all type params
        TypeSubstContext subst_ctx;
        subst_ctx.type_params = template->as.struct_decl.type_params;
        subst_ctx.type_args   = info->type_args;
        subst_ctx.count       = template->as.struct_decl.type_param_count;
        gen->generics.subst   = &subst_ctx;

        // Emit fields with substituted types
        for (int f = 0; f < template->as.struct_decl.fields.count; f++) {
            Node* field = template->as.struct_decl.fields.nodes[f];
            emit_indent(gen);
            emit_type(gen, field->as.field.type);
            emit(gen, " %s;\n", field->as.field.name);
        }

        gen->generics.subst = NULL;

        gen->out.indent--;
        emit(gen, "} %s;\n\n", info->mangled_name);
    }
}

// Emit forward declarations for all functions, methods, and generic method instances
void emit_function_forward_decls(CodeGen* gen, Node* ast) {
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

                emit_func_return_type(gen, fdn);

                // Emit function name with appropriate prefix
                emit_function_name(gen, fdn->name, is_method ? fdn->receiver_type : NULL,
                                   module_prefix);

                // Emit self parameter for methods
                if (is_method) {
                    if (fdn->receiver_is_const && strcmp(fdn->receiver_type, "string") != 0) {
                        emit(gen, "const ");
                    }
                    if (type_is_builtin_name(fdn->receiver_type)) {
                        emit(gen, "%s self", type_c_name(fdn->receiver_type));
                    } else {
                        emit(gen, "%s* self", fdn->receiver_type);
                    }
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
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* info = &gen->checker.instances[i];

        // Find the generic type template to get type params
        Node* template    = find_generic_struct_decl(ast, info->base_name);
        int   is_struct   = 0;
        int   param_count = 0;
        if (template) {
            is_struct   = 1;
            param_count = template->as.struct_decl.type_param_count;
        } else {
            template = find_generic_enum_decl(ast, info->base_name);
            if (template) {
                param_count = template->as.enum_decl.type_param_count;
            }
        }
        if (!template)
            continue;

        // Find all methods for this generic type
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
            int    combined_count  = param_count + method_bind_count;
            char** combined_params = xmalloc(combined_count * sizeof(char*));
            Type** combined_args   = xmalloc(combined_count * sizeof(Type*));

            for (int k = 0; k < param_count; k++) {
                if (is_struct) {
                    combined_params[k] = template->as.struct_decl.type_params[k];
                } else {
                    combined_params[k] = template->as.enum_decl.type_params[k];
                }
                combined_args[k] = info->type_args[k];
            }
            for (int k = 0; k < method_bind_count; k++) {
                combined_params[param_count + k] = method_params[k];
                combined_args[param_count + k]   = method_args[k];
            }

            TypeSubstContext subst_ctx;
            subst_ctx.type_params = combined_params;
            subst_ctx.type_args   = combined_args;
            subst_ctx.count       = combined_count;
            gen->generics.subst   = &subst_ctx;

            // Return type (with substitution)
            emit_func_return_type(gen, fdn);

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

            gen->generics.subst = NULL;
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
}
