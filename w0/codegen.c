#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_emit.h"
#include "types.h"
#include "vec.h"

// Check if two tuple types are structurally equal
int tuple_types_equal(Type* a, Type* b) {
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
    Type* type = pattern->resolved_type;
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
Type* type_from_node(Node* type_node) {
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
char* build_mangled_name_from_generic_node(CodeGen* gen, Node* type_node) {
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

// Initialize the code generator with output stream, checker results, and RC debug flag
void codegen_init(CodeGen* gen, FILE* out, GenericInstance* generic_instances, int generic_count,
                  SpanInstance* span_instances, int span_count, VecInstance* vec_instances,
                  int vec_count, TraitImpl* trait_impls, int trait_impl_count, int rc_debug) {
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
    gen->vec_instances          = vec_instances;
    gen->vec_instance_count     = vec_count;
    gen->trait_impls            = trait_impls;
    gen->trait_impl_count       = trait_impl_count;
    gen->enum_names             = NULL;
    gen->enum_name_count        = 0;
    gen->enum_name_capacity     = 0;
    gen->enum_has_rc_fields     = NULL;
    gen->alias_names            = NULL;
    gen->alias_targets          = NULL;
    gen->alias_count            = 0;
    gen->alias_capacity         = 0;
}

// Collect tuple types from all declarations and register non-generic type aliases
static void collect_types_and_aliases(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            collect_tuple_types_from_decl(gen, decl);
            // Collect non-generic type aliases for codegen resolution
            if (decl->type == NODE_TYPE_ALIAS && decl->as.type_alias.type_param_count == 0) {
                VEC_GROW(gen->alias_names, gen->alias_count, gen->alias_capacity);
                // Grow alias_targets in parallel (realloc to match capacity)
                gen->alias_targets =
                    xrealloc(gen->alias_targets, gen->alias_capacity * sizeof(Node*));
                gen->alias_names[gen->alias_count]   = decl->as.type_alias.name;
                gen->alias_targets[gen->alias_count] = decl->as.type_alias.target_type;
                gen->alias_count++;
            }
        }
    }
}

// Emit the standard C header includes for the generated output
static void emit_c_headers(CodeGen* gen) {
    emit(gen, "/* Generated by whist compiler */\n");
    emit(gen, "#include <stdint.h>\n");
    emit(gen, "#include <stdbool.h>\n");
    emit(gen, "#include <stddef.h>\n");
    emit(gen, "#include <stdlib.h>\n");
    emit(gen, "#include <stdio.h>\n");
    emit(gen, "#include <string.h>\n");
    emit(gen, "#include <stdarg.h>\n");
    emit(gen, "\n");
}

// Emit the reference counting runtime: __RcHeader, __rc_alloc, __rc_inc, __rc_dec
static void emit_rc_runtime(CodeGen* gen) {
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
}

// Emit bounds-checking helper functions for span and vec indexing
static void emit_bounds_checks(CodeGen* gen) {
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

    if (gen->vec_instance_count > 0) {
        emit(gen, "static inline void __w0_vec_check(int64_t count, int64_t idx, int line, int "
                  "col) {\n");
        emit(gen, "    if (idx < 0 || idx >= count) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: Vec index %%lld out of bounds (count=%%lld) "
                  "at %%d:%%d\\n\",\n");
        emit(gen, "                (long long)idx, (long long)count, line, col);\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");
    }
}

// Emit string method helpers
static void emit_string_helpers(CodeGen* gen) {
    emit(gen, "static inline int64_t __String_length(const char* s) { return (int64_t)strlen(s); "
              "}\n");
    emit(gen, "static inline const char* __String_concat(const char* a, const char* b) {\n");
    emit(gen, "    size_t la = strlen(a), lb = strlen(b);\n");
    emit(gen, "    char* r = (char*)malloc(la + lb + 1);\n");
    emit(gen, "    memcpy(r, a, la); memcpy(r + la, b, lb + 1);\n");
    emit(gen, "    return r;\n");
    emit(gen, "}\n");
    emit(gen, "static inline const char* __String_substr(const char* s, int64_t start, int64_t "
              "end) {\n");
    emit(gen, "    int64_t len = end - start;\n");
    emit(gen, "    if (len < 0) len = 0;\n");
    emit(gen, "    char* r = (char*)malloc(len + 1);\n");
    emit(gen, "    memcpy(r, s + start, len);\n");
    emit(gen, "    r[len] = '\\0';\n");
    emit(gen, "    return r;\n");
    emit(gen, "}\n");
    emit(gen, "static inline bool __String_contains(const char* s, const char* sub) {\n");
    emit(gen, "    return strstr(s, sub) != NULL;\n");
    emit(gen, "}\n");
    emit(gen, "static inline bool __String_starts_with(const char* s, const char* prefix) {\n");
    emit(gen, "    return strncmp(s, prefix, strlen(prefix)) == 0;\n");
    emit(gen, "}\n");
    emit(gen, "static inline bool __String_ends_with(const char* s, const char* suffix) {\n");
    emit(gen, "    size_t ls = strlen(s), lsuf = strlen(suffix);\n");
    emit(gen, "    return ls >= lsuf && strcmp(s + ls - lsuf, suffix) == 0;\n");
    emit(gen, "}\n");
    emit(gen, "static inline const char* __std_format(const char* fmt, ...) {\n");
    emit(gen, "    va_list args1, args2;\n");
    emit(gen, "    va_start(args1, fmt); va_copy(args2, args1);\n");
    emit(gen, "    int n = vsnprintf(NULL, 0, fmt, args1);\n");
    emit(gen, "    va_end(args1);\n");
    emit(gen, "    char* buf = (char*)malloc(n + 1);\n");
    emit(gen, "    vsnprintf(buf, n + 1, fmt, args2);\n");
    emit(gen, "    va_end(args2);\n");
    emit(gen, "    return buf;\n");
    emit(gen, "}\n\n");
}

// Emit C struct typedefs for each collected tuple type (__tuple_tN)
static void emit_tuple_typedefs(CodeGen* gen) {
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

// Build the list of all enum type names (non-generic and generic instances)
static void register_enum_names(CodeGen* gen, Node* ast) {
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
}

// Compute which enums contain RC-managed fields (fixed-point iteration)
static void compute_enum_rc_flags(CodeGen* gen, Node* ast) {
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
}

// Emit forward typedef declarations for all structs (non-generic and instantiated)
static void emit_struct_forward_decls(CodeGen* gen, Node* ast) {
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
}

// Emit struct typedefs for each instantiated Span type (__Span_T)
static void emit_span_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->span_instance_count; i++) {
        SpanInstance* inst = &gen->span_instances[i];
        emit(gen, "typedef struct {\n");
        emit(gen, "    const ");
        emit_resolved_type(gen, inst->elem_type);
        emit(gen, "* data;\n");
        emit(gen, "    uint64_t count;\n");
        emit(gen, "} __Span_%s;\n\n", type_name(inst->elem_type));
    }
}

// Emit struct typedefs for each instantiated Vec type (__Vec_T)
static void emit_vec_typedefs(CodeGen* gen) {
    for (int i = 0; i < gen->vec_instance_count; i++) {
        VecInstance* inst       = &gen->vec_instances[i];
        Type*        elem_type  = inst->elem_type;
        const char*  elem_tname = type_name(elem_type);

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
static void emit_enum_typedefs(CodeGen* gen, Node* ast) {
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
}

// Emit C enum/tagged-union typedefs for instantiated generic enum types
static void emit_generic_enum_typedefs(CodeGen* gen, Node* ast) {
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
}

// Emit __rc_inc_EnumName/__rc_dec_EnumName helpers for enums with RC-managed fields
static void emit_enum_rc_helpers(CodeGen* gen, Node* ast) {
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
}

// Emit struct body typedefs (field definitions) for non-generic and generic structs
static void emit_struct_body_typedefs(CodeGen* gen, Node* ast) {
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

        // Set up substitution context so emit_type handles all type params
        TypeSubstContext subst_ctx;
        subst_ctx.type_params = template->as.struct_decl.type_params;
        subst_ctx.type_args   = info->type_args;
        subst_ctx.count       = template->as.struct_decl.type_param_count;
        gen->subst_ctx        = &subst_ctx;

        // Emit fields with substituted types
        for (int f = 0; f < template->as.struct_decl.fields.count; f++) {
            Node* field = template->as.struct_decl.fields.nodes[f];
            emit_indent(gen);
            emit_type(gen, field->as.field.type);
            emit(gen, " %s;\n", field->as.field.name);
        }

        gen->subst_ctx = NULL;

        gen->indent--;
        emit(gen, "} %s;\n\n", info->mangled_name);
    }
}

// Emit forward declarations for all functions, methods, and generic method instances
static void emit_function_forward_decls(CodeGen* gen, Node* ast) {
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
}

// Check if a struct type has a Drop trait implementation
static int struct_implements_drop(CodeGen* gen, const char* type_name) {
    for (int t = 0; t < gen->trait_impl_count; t++) {
        if (strcmp(gen->trait_impls[t].trait_name, "Drop") == 0 &&
            strcmp(gen->trait_impls[t].type_name, type_name) == 0)
            return 1;
    }
    return 0;
}

typedef struct {
    char* field_name;
    char* type_name;
    int   is_enum;
} RcFieldInfo;

// Collect info about RC-managed fields in a struct for __rc_dec generation
static int collect_rc_field_info(CodeGen* gen, Node* struct_decl, RcFieldInfo** out_fields) {
    int          count  = 0;
    RcFieldInfo* fields = NULL;
    for (int f = 0; f < struct_decl->as.struct_decl.fields.count; f++) {
        Node* field = struct_decl->as.struct_decl.fields.nodes[f];
        if (!field->as.field.type || !type_node_has_rc(gen, field->as.field.type))
            continue;
        count++;
        fields            = xrealloc(fields, count * sizeof(RcFieldInfo));
        RcFieldInfo* info = &fields[count - 1];
        info->field_name  = xstrdup(field->as.field.name);
        if (field->as.field.type->type == NODE_IDENT) {
            info->type_name = xstrdup(field->as.field.type->as.ident.name);
            info->is_enum   = is_enum_type_name(gen, field->as.field.type->as.ident.name);
        } else if (field->as.field.type->type == NODE_GENERIC_TYPE) {
            Node*  gtype     = field->as.field.type;
            int    arg_count = gtype->as.generic_type.type_args.count;
            Type** args      = xmalloc(arg_count * sizeof(Type*));
            for (int a = 0; a < arg_count; a++) {
                args[a] = type_from_node(gtype->as.generic_type.type_args.nodes[a]);
            }
            info->type_name =
                type_mangle_generic(gtype->as.generic_type.base_name, args, arg_count);
            free(args);
            info->is_enum = 0;
        } else {
            info->type_name = NULL;
            info->is_enum   = 0;
        }
    }
    *out_fields = fields;
    return count;
}

// Check if a field type needs a type-specific __rc_dec_TypeName (has Drop or RC fields)
static int field_needs_custom_dec(CodeGen* gen, const char* field_type_name, Node* ast) {
    if (!field_type_name)
        return 0;
    if (struct_implements_drop(gen, field_type_name))
        return 1;
    // Check if field type has RC fields (scan non-generic structs)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL && decl->as.struct_decl.type_param_count == 0 &&
                strcmp(decl->as.struct_decl.name, field_type_name) == 0) {
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    if (field->as.field.type && type_node_has_rc(gen, field->as.field.type))
                        return 1;
                }
                return 0;
            }
        }
    }
    // Check generic instances (e.g., field type "Box_Inner")
    for (int i = 0; i < gen->generic_instance_count; i++) {
        if (strcmp(gen->generic_instances[i].mangled_name, field_type_name) == 0) {
            Type* t = gen->generic_instances[i].type;
            if (t && t->kind == TYPE_STRUCT && (t->as.struc.has_drop || t->as.struc.has_rc_fields))
                return 1;
            return 0;
        }
    }
    return 0;
}

// Emit forward declarations for __rc_dec_TypeName functions (needed for cross-references)
static void emit_struct_rc_dec_forward_decls(CodeGen* gen, Node* ast) {
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
            const char* sname     = decl->as.struct_decl.name;
            int         needs_dec = struct_implements_drop(gen, sname);
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
        if (struct_implements_drop(gen, info->base_name) || t->as.struc.has_rc_fields) {
            emit(gen, "static inline void __rc_dec_%s(%s* ptr);\n", info->mangled_name,
                 info->mangled_name);
        }
    }
    emit(gen, "\n");
}

// Emit push, pop, and clear method implementations for each Vec type instance
static void emit_vec_methods(CodeGen* gen) {
    for (int i = 0; i < gen->vec_instance_count; i++) {
        VecInstance* inst        = &gen->vec_instances[i];
        Type*        elem_type   = inst->elem_type;
        const char*  elem_tname  = type_name(elem_type);
        int          elem_is_ptr = (elem_type->kind == TYPE_STRUCT || elem_type->kind == TYPE_VEC);

        // Push
        emit(gen, "static inline void __Vec_%s_push(__Vec_%s* self, ", elem_tname, elem_tname);
        emit_resolved_type(gen, elem_type);
        emit(gen, " value) {\n");
        emit(gen, "    if (self->count == self->capacity) {\n");
        emit(gen, "        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;\n");
        emit(gen, "        self->data = realloc(self->data, new_cap * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "        self->capacity = new_cap;\n");
        emit(gen, "    }\n");
        emit(gen, "    self->data[self->count] = value;\n");
        emit(gen, "    self->count++;\n");
        emit(gen, "}\n\n");

        // Pop
        emit(gen, "static inline ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " __Vec_%s_pop(__Vec_%s* self) {\n", elem_tname, elem_tname);
        emit(gen, "    if (self->count == 0) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: pop from empty Vec\\n\");\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "    self->count--;\n");
        emit(gen, "    return self->data[self->count];\n");
        emit(gen, "}\n\n");

        // Clear
        emit(gen, "static inline void __Vec_%s_clear(__Vec_%s* self) {\n", elem_tname, elem_tname);
        if (elem_is_ptr) {
            emit(gen, "    for (int64_t i = 0; i < self->count; i++) {\n");
            if (elem_type->kind == TYPE_VEC) {
                emit(gen, "        __rc_dec_Vec_%s(self->data[i]);\n",
                     type_name(elem_type->as.vec.elem));
            } else if (elem_type->kind == TYPE_STRUCT &&
                       (elem_type->as.struc.has_drop || elem_type->as.struc.has_rc_fields)) {
                emit(gen, "        __rc_dec_%s(self->data[i]);\n", elem_type->as.struc.name);
            } else {
                emit(gen, "        __rc_dec(self->data[i]);\n");
            }
            emit(gen, "    }\n");
        }
        emit(gen, "    self->count = 0;\n");
        emit(gen, "}\n\n");
    }
}

// Emit __rc_dec_Vec_T functions that free Vec elements and the Vec itself
static void emit_vec_rc_dec(CodeGen* gen) {
    for (int i = 0; i < gen->vec_instance_count; i++) {
        VecInstance* inst       = &gen->vec_instances[i];
        const char*  elem_tname = type_name(inst->elem_type);
        emit(gen, "static inline void __rc_dec_Vec_%s(__Vec_%s* ptr);\n", elem_tname, elem_tname);
    }

    // Emit __rc_dec_Vec_* definitions
    for (int i = 0; i < gen->vec_instance_count; i++) {
        VecInstance* inst        = &gen->vec_instances[i];
        Type*        elem_type   = inst->elem_type;
        const char*  elem_tname  = type_name(elem_type);
        int          elem_is_ptr = (elem_type->kind == TYPE_STRUCT || elem_type->kind == TYPE_VEC);

        emit(gen, "static inline void __rc_dec_Vec_%s(__Vec_%s* ptr) {\n", elem_tname, elem_tname);
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
        emit(gen, "    if (--h->refcount == 0) {\n");
        if (elem_is_ptr) {
            emit(gen, "        for (int64_t i = 0; i < ptr->count; i++) {\n");
            if (elem_type->kind == TYPE_VEC) {
                emit(gen, "            __rc_dec_Vec_%s(ptr->data[i]);\n",
                     type_name(elem_type->as.vec.elem));
            } else if (elem_type->kind == TYPE_STRUCT &&
                       (elem_type->as.struc.has_drop || elem_type->as.struc.has_rc_fields)) {
                emit(gen, "            __rc_dec_%s(ptr->data[i]);\n", elem_type->as.struc.name);
            } else {
                emit(gen, "            __rc_dec(ptr->data[i]);\n");
            }
            emit(gen, "        }\n");
        }
        emit(gen, "        free(ptr->data);\n");
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
}

// Emit __rc_dec_TypeName definitions for structs with Drop or RC-managed fields
static void emit_struct_rc_dec(CodeGen* gen, Node* ast) {
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

            const char* sname    = decl->as.struct_decl.name;
            int         has_drop = struct_implements_drop(gen, sname);

            RcFieldInfo* rc_fields      = NULL;
            int          rc_field_count = collect_rc_field_info(gen, decl, &rc_fields);

            if (!has_drop && rc_field_count == 0)
                continue;

            emit(gen, "static inline void __rc_dec_%s(%s* ptr) {\n", sname, sname);
            emit(gen, "    if (!ptr) return;\n");
            emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
            emit(gen, "    if (--h->refcount == 0) {\n");
            if (has_drop) {
                emit(gen, "        %s_drop(ptr);\n", sname);
            }
            for (int f = 0; f < rc_field_count; f++) {
                const char* field_tname = rc_fields[f].type_name;
                if (rc_fields[f].is_enum) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", field_tname,
                         rc_fields[f].field_name);
                } else if (field_needs_custom_dec(gen, field_tname, ast)) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", field_tname,
                         rc_fields[f].field_name);
                } else {
                    emit(gen, "        __rc_dec(ptr->%s);\n", rc_fields[f].field_name);
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
                free(rc_fields[f].field_name);
                free(rc_fields[f].type_name);
            }
            free(rc_fields);
        }
    }

    // Generic instances: emit __rc_dec_MangledName for those with Drop or RC fields
    for (int i = 0; i < gen->generic_instance_count; i++) {
        GenericInstance* info = &gen->generic_instances[i];
        Type*            t    = info->type;
        if (!t || t->kind != TYPE_STRUCT)
            continue;

        int has_drop      = struct_implements_drop(gen, info->base_name);
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
            } else if (ft && ft->kind == TYPE_VEC) {
                emit(gen, "        __rc_dec_Vec_%s(ptr->%s);\n", type_name(ft->as.vec.elem),
                     t->as.struc.field_names[f]);
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
}

// Emit all top-level declarations across all modules
static void emit_declarations(CodeGen* gen, Node* ast) {
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
}

// Emit implementations for all instantiated generic struct methods
static void emit_generic_method_impls(CodeGen* gen, Node* ast) {
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
            gen->current_return_type      = fdn->return_type;
            gen->current_generic_template = template;
            gen->accessible_modules       = fdn->accessible_modules;
            gen->accessible_modules_count = fdn->accessible_modules_count;

            // Use per-instantiation cloned body if available, otherwise fall back
            // to the shared template body
            Node* method_body = fdn->body;
            if (info->method_bodies && j < info->method_body_count && info->method_bodies[j]) {
                method_body = info->method_bodies[j];
            }

            // First pass: count defers to know if we need __ret
            int has_defers = 0;
            if (method_body) {
                for (int s = 0; s < method_body->as.block.stmts.count; s++) {
                    Node* stmt = method_body->as.block.stmts.nodes[s];
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
            if (method_body) {
                for (int s = 0; s < method_body->as.block.stmts.count; s++) {
                    emit_stmt(gen, method_body->as.block.stmts.nodes[s]);
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

            gen->subst_ctx                = NULL;
            gen->current_generic_template = NULL;
            gen->accessible_modules       = NULL;
            gen->accessible_modules_count = 0;
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

// Main codegen entry point: emit all C code for a program AST
void codegen_emit(CodeGen* gen, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM)
        return;

    collect_types_and_aliases(gen, ast);
    emit_c_headers(gen);
    emit_rc_runtime(gen);
    emit_bounds_checks(gen);
    emit_string_helpers(gen);
    emit_tuple_typedefs(gen);
    register_enum_names(gen, ast);
    compute_enum_rc_flags(gen, ast);
    emit_struct_forward_decls(gen, ast);
    emit_span_typedefs(gen);
    emit_vec_typedefs(gen);
    emit_enum_typedefs(gen, ast);
    emit_generic_enum_typedefs(gen, ast);
    emit_enum_rc_helpers(gen, ast);
    emit_struct_body_typedefs(gen, ast);
    emit_function_forward_decls(gen, ast);
    emit_struct_rc_dec_forward_decls(gen, ast);
    emit_vec_methods(gen);
    emit_vec_rc_dec(gen);
    emit_struct_rc_dec(gen, ast);
    emit_declarations(gen, ast);
    emit_generic_method_impls(gen, ast);
}
