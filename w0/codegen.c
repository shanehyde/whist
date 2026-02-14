#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_emit.h"
#include "codegen_types.h"
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
Node* find_generic_struct_decl(Node* ast, const char* name) {
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
Node* find_generic_enum_decl(Node* ast, const char* name) {
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

// Look up a type parameter name in the current generic substitution context.
// Returns the concrete Type* if found, or NULL if no substitution applies.
Type* subst_lookup(CodeGen* gen, const char* name) {
    if (!gen->generics.subst)
        return NULL;
    for (int i = 0; i < gen->generics.subst->count; i++) {
        if (strcmp(gen->generics.subst->type_params[i], name) == 0)
            return gen->generics.subst->type_args[i];
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
            const char* arg_name = arg->as.ident.name;
            Type*       resolved = subst_lookup(gen, arg_name);
            if (resolved) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", type_name(resolved));
            } else {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", arg_name);
            }
        } else if (arg->type == NODE_GENERIC_TYPE) {
            pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", arg->as.generic_type.base_name);
            for (int j = 0; j < arg->as.generic_type.type_args.count; j++) {
                pos += snprintf(buf + pos, sizeof(buf) - pos, "_");
                Node* nested = arg->as.generic_type.type_args.nodes[j];
                if (nested->type == NODE_IDENT) {
                    const char* nested_name = nested->as.ident.name;
                    Type*       resolved_n  = subst_lookup(gen, nested_name);
                    if (resolved_n) {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", type_name(resolved_n));
                    } else {
                        pos += snprintf(buf + pos, sizeof(buf) - pos, "%s", nested_name);
                    }
                }
            }
        }
    }
    if (pos >= (int)sizeof(buf)) {
        fprintf(stderr, "fatal: mangled generic name exceeds %d bytes\n", (int)sizeof(buf));
        exit(1);
    }
    return xstrdup(buf);
}

// Helper to get the type parameter name for mangling (currently unused)
// static const char* type_arg_mangle_name(Type* type) {
//     ... preserved for potential future use
// }

// Collect all generic methods for a given struct name
void collect_generic_methods(Node* ast, const char* struct_name, Node*** methods_out,
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
void codegen_init(CodeGen* gen, FILE* out, CodeGenChecker checker_data, int rc_debug, int test_mode,
                  const char* source_file) {
    gen->out.file       = out;
    gen->out.indent     = 0;
    gen->out.temp_count = 0;

    gen->defer.stack       = NULL;
    gen->defer.count       = 0;
    gen->defer.capacity    = 0;
    gen->defer.return_type = NULL;

    gen->rc.vars     = NULL;
    gen->rc.count    = 0;
    gen->rc.capacity = 0;
    gen->rc.depth    = 0;
    gen->rc.debug    = rc_debug;

    gen->generics.subst        = NULL;
    gen->generics.tmpl         = NULL;
    gen->generics.modules      = NULL;
    gen->generics.module_count = 0;

    gen->checker = checker_data;

    gen->enums.names         = NULL;
    gen->enums.count         = 0;
    gen->enums.capacity      = 0;
    gen->enums.has_rc_fields = NULL;
    gen->enums.has_eq        = NULL;

    gen->aliases.types                = NULL;
    gen->aliases.type_targets         = NULL;
    gen->aliases.type_count           = 0;
    gen->aliases.type_capacity        = 0;
    gen->aliases.externs              = NULL;
    gen->aliases.extern_count         = 0;
    gen->aliases.extern_capacity      = 0;
    gen->aliases.extern_funcs         = NULL;
    gen->aliases.extern_func_count    = 0;
    gen->aliases.extern_func_capacity = 0;
    gen->aliases.uses                 = NULL;
    gen->aliases.use_count            = 0;
    gen->aliases.use_capacity         = 0;

    gen->tuple_types         = NULL;
    gen->tuple_type_count    = 0;
    gen->tuple_type_capacity = 0;
    gen->current_module      = NULL;
    gen->in_enum_method      = 0;
    gen->test_mode           = test_mode;
    gen->test_index          = 0;
    gen->source_file         = source_file;
}

void codegen_free(CodeGen* gen) {
    free(gen->defer.stack);
    for (int i = 0; i < gen->rc.count; i++) {
        free(gen->rc.vars[i].name);
        free(gen->rc.vars[i].dec_func);
    }
    free(gen->rc.vars);
    for (int i = 0; i < gen->enums.count; i++) {
        free(gen->enums.names[i]);
    }
    free(gen->enums.names);
    free(gen->enums.has_rc_fields);
    free(gen->enums.has_eq);
    free(gen->aliases.types);
    free(gen->aliases.type_targets);
    free(gen->aliases.externs);
    free(gen->aliases.extern_funcs);
    for (int i = 0; i < gen->aliases.use_count; i++) {
        free(gen->aliases.uses[i].whist_name);
        free(gen->aliases.uses[i].c_name);
    }
    free(gen->aliases.uses);
    free(gen->tuple_types);
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
                VEC_GROW(gen->aliases.types, gen->aliases.type_count, gen->aliases.type_capacity);
                // Grow alias_targets in parallel (realloc to match capacity)
                gen->aliases.type_targets =
                    xrealloc(gen->aliases.type_targets, gen->aliases.type_capacity * sizeof(Node*));
                gen->aliases.types[gen->aliases.type_count] = decl->as.type_alias.name;
                gen->aliases.type_targets[gen->aliases.type_count] =
                    decl->as.type_alias.target_type;
                gen->aliases.type_count++;
            }
        }
    }
}

// Emit the standard C header includes for the generated output
static void emit_c_headers(CodeGen* gen) {
    emit(gen, "/* Generated by whist compiler */\n");
    if (gen->rc.debug) {
        emit(gen, "#define WHIST_RC_DEBUG\n");
    }
    emit(gen, "#include <whist_runtime.h>\n");
    if (gen->test_mode) {
        emit(gen, "#include <setjmp.h>\n");
        emit(gen, "static jmp_buf __test_jmp_buf;\n");
    }
    emit(gen, "\n");
}

// Return 1 if the program declares a top-level `main` function in the main module.
static int program_has_user_main(Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        if (strcmp(mod->as.module.name, "main") != 0)
            continue;

        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (!decl || decl->type != NODE_FUNC_DECL)
                continue;
            if (decl->as.func_decl.receiver_type != NULL)
                continue;
            if (strcmp(decl->as.func_decl.name, "main") == 0)
                return 1;
        }
    }
    return 0;
}

// Emit the real C entrypoint wrapper that captures argc/argv for std.args().
static void emit_main_wrapper(CodeGen* gen) {
    emit(gen, "int main(int argc, char** argv) {\n");
    emit(gen, "    __w0_argc = argc;\n");
    emit(gen, "    __w0_argv = argv;\n");
    emit(gen, "    return __w0_user_main();\n");
    emit(gen, "}\n\n");
}

// Stringify an expression AST node into a human-readable string for assert messages
static void stringify_expr_to(Node* node, char* buf, int buf_size, int* pos) {
    if (!node || *pos >= buf_size - 1)
        return;

#define APPEND(s)                                                                                  \
    do {                                                                                           \
        const char* _s = (s);                                                                      \
        while (*_s && *pos < buf_size - 1)                                                         \
            buf[(*pos)++] = *_s++;                                                                 \
    } while (0)

    switch (node->type) {
    case NODE_INT_LIT: {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%ld", node->as.int_lit.value);
        APPEND(tmp);
        break;
    }
    case NODE_FLOAT_LIT: {
        char tmp[32];
        snprintf(tmp, sizeof(tmp), "%g", node->as.float_lit.value);
        APPEND(tmp);
        break;
    }
    case NODE_BOOL_LIT:
        APPEND(node->as.bool_lit.value ? "true" : "false");
        break;
    case NODE_STRING_LIT:
        APPEND("\"");
        APPEND(node->as.string_lit.value);
        APPEND("\"");
        break;
    case NODE_NULL_LIT:
        APPEND("null");
        break;
    case NODE_IDENT:
        for (int i = 0; i < node->as.ident.length && *pos < buf_size - 1; i++)
            buf[(*pos)++] = node->as.ident.name[i];
        break;
    case NODE_BINARY: {
        APPEND("(");
        stringify_expr_to(node->as.binary.left, buf, buf_size, pos);
        APPEND(" ");
        APPEND(token_type_symbol(node->as.binary.op));
        APPEND(" ");
        stringify_expr_to(node->as.binary.right, buf, buf_size, pos);
        APPEND(")");
        break;
    }
    case NODE_UNARY:
        APPEND(token_type_symbol(node->as.unary.op));
        stringify_expr_to(node->as.unary.operand, buf, buf_size, pos);
        break;
    case NODE_CALL: {
        stringify_expr_to(node->as.call.func, buf, buf_size, pos);
        APPEND("(");
        for (int i = 0; i < node->as.call.args.count; i++) {
            if (i > 0)
                APPEND(", ");
            stringify_expr_to(node->as.call.args.nodes[i], buf, buf_size, pos);
        }
        APPEND(")");
        break;
    }
    case NODE_MEMBER:
        stringify_expr_to(node->as.member.object, buf, buf_size, pos);
        APPEND(".");
        for (int i = 0; i < node->as.member.length && *pos < buf_size - 1; i++)
            buf[(*pos)++] = node->as.member.name[i];
        break;
    case NODE_INDEX:
        stringify_expr_to(node->as.index.object, buf, buf_size, pos);
        APPEND("[");
        stringify_expr_to(node->as.index.index, buf, buf_size, pos);
        APPEND("]");
        break;
    default:
        APPEND("...");
        break;
    }
#undef APPEND
}

char* stringify_expr(Node* node) {
    static char buf[512];
    int         pos = 0;
    stringify_expr_to(node, buf, sizeof(buf), &pos);
    buf[pos] = '\0';
    return buf;
}

// Collect all NODE_TEST_DECL nodes from the program AST
typedef struct {
    Node** tests;
    int    count;
    int    capacity;
} TestCollector;

static void collect_test_decls(Node* ast, TestCollector* tc) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        if (strcmp(mod->as.module.name, "main") != 0)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl && decl->type == NODE_TEST_DECL) {
                VEC_GROW(tc->tests, tc->count, tc->capacity);
                tc->tests[tc->count++] = decl;
            }
        }
    }
}

// Emit the test runner main function
static void emit_test_runner(CodeGen* gen, Node* ast) {
    TestCollector tc = {NULL, 0, 0};
    collect_test_decls(ast, &tc);

    emit(gen, "int main(int argc, char** argv) {\n");
    emit(gen, "    __w0_argc = argc;\n");
    emit(gen, "    __w0_argv = argv;\n");
    emit(gen, "    int __passed = 0, __failed = 0;\n");

    for (int i = 0; i < tc.count; i++) {
        Node* t = tc.tests[i];
        emit(gen, "    if (setjmp(__test_jmp_buf) == 0) {\n");
        emit(gen, "        __test_%d();\n", i);
        emit(gen, "        __passed++;\n");
        emit(gen, "        fprintf(stderr, \"PASS: %.*s\\n\");\n", t->as.test_decl.name_length,
             t->as.test_decl.name);
        emit(gen, "    } else {\n");
        emit(gen, "        __failed++;\n");
        emit(gen, "        fprintf(stderr, \"FAIL: %.*s\\n\");\n", t->as.test_decl.name_length,
             t->as.test_decl.name);
        emit(gen, "    }\n");
    }

    emit(gen, "    fprintf(stderr, \"\\n%%d passed, %%d failed, %%d total\\n\", __passed, "
              "__failed, __passed + __failed);\n");
    emit(gen, "    return __failed > 0 ? 1 : 0;\n");
    emit(gen, "}\n");

    free(tc.tests);
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
            VEC_GROW(gen->enums.names, gen->enums.count, gen->enums.capacity);
            gen->enums.names[gen->enums.count++] = xstrdup(decl->as.enum_decl.name);
        }
    }

    // Register generic enum instance names
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* inst = &gen->checker.instances[i];
        if (inst->type->kind != TYPE_ENUM)
            continue;
        VEC_GROW(gen->enums.names, gen->enums.count, gen->enums.capacity);
        gen->enums.names[gen->enums.count++] = xstrdup(inst->mangled_name);
    }
}

// Compute which enums contain RC-managed fields (fixed-point iteration)
static void compute_enum_rc_flags(CodeGen* gen, Node* ast) {
    if (gen->enums.count > 0) {
        gen->enums.has_rc_fields = xcalloc(gen->enums.count, sizeof(int));
        int changed              = 1;
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
                    if (idx < 0 || gen->enums.has_rc_fields[idx])
                        continue;
                    for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
                        Node* var = decl->as.enum_decl.values.nodes[v];
                        for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                            if (type_node_has_rc(gen, var->as.enum_variant.types.nodes[t])) {
                                gen->enums.has_rc_fields[idx] = 1;
                                changed                       = 1;
                                break;
                            }
                        }
                        if (gen->enums.has_rc_fields[idx])
                            break;
                    }
                }
            }
        }
        // Set RC flags for generic enum instances from checker Type data
        for (int i = 0; i < gen->checker.instance_count; i++) {
            GenericInstance* inst = &gen->checker.instances[i];
            if (inst->type->kind != TYPE_ENUM)
                continue;
            int idx = enum_index(gen, inst->mangled_name);
            if (idx >= 0 && inst->type->as.enm.has_rc_fields) {
                gen->enums.has_rc_fields[idx] = 1;
            }
        }
    }
}

// Check if a struct type name has an Eq trait implementation
static int struct_has_eq_impl(CodeGen* gen, const char* name) {
    for (int t = 0; t < gen->checker.trait_count; t++) {
        if (strcmp(gen->checker.traits[t].trait_name, "Eq") == 0 &&
            strcmp(gen->checker.traits[t].type_name, name) == 0)
            return 1;
    }
    return 0;
}

// Compute which data enums support equality (all struct-typed variant fields have Eq)
static void compute_enum_eq_flags(CodeGen* gen, Node* ast) {
    if (gen->enums.count == 0)
        return;
    gen->enums.has_eq = xcalloc(gen->enums.count, sizeof(int));

    // Non-generic data enums
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue;
            int idx = enum_index(gen, decl->as.enum_decl.name);
            if (idx < 0)
                continue;
            // Check if data enum
            int has_data = 0;
            for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
                if (decl->as.enum_decl.values.nodes[v]->as.enum_variant.types.count > 0) {
                    has_data = 1;
                    break;
                }
            }
            if (!has_data)
                continue;
            // Check all struct-typed fields have Eq
            int all_eq = 1;
            for (int v = 0; v < decl->as.enum_decl.values.count && all_eq; v++) {
                Node* var = decl->as.enum_decl.values.nodes[v];
                for (int t = 0; t < var->as.enum_variant.types.count; t++) {
                    Node* tnode = var->as.enum_variant.types.nodes[t];
                    if (is_struct_type(gen, tnode)) {
                        Node*       resolved = resolve_alias(gen, tnode);
                        const char* sname    = NULL;
                        if (resolved->type == NODE_IDENT)
                            sname = resolved->as.ident.name;
                        else if (resolved->type == NODE_GENERIC_TYPE)
                            sname = build_mangled_name_from_generic_node(gen, resolved);
                        if (!sname || !struct_has_eq_impl(gen, sname)) {
                            all_eq = 0;
                            if (resolved->type == NODE_GENERIC_TYPE && sname)
                                free((char*)sname);
                            break;
                        }
                        if (resolved->type == NODE_GENERIC_TYPE)
                            free((char*)sname);
                    }
                }
            }
            if (all_eq)
                gen->enums.has_eq[idx] = 1;
        }
    }

    // Generic data enum instances: use checker Type info
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* inst = &gen->checker.instances[gi];
        if (inst->type->kind != TYPE_ENUM || !inst->type->as.enm.has_data)
            continue;
        int idx = enum_index(gen, inst->mangled_name);
        if (idx < 0)
            continue;
        int all_eq = 1;
        for (int v = 0; v < inst->type->as.enm.value_count && all_eq; v++) {
            for (int f = 0; f < inst->type->as.enm.variant_type_counts[v]; f++) {
                Type* ft = inst->type->as.enm.variant_types[v][f];
                if (ft->kind == TYPE_STRUCT && !ft->as.struc.has_eq) {
                    all_eq = 0;
                    break;
                }
            }
        }
        if (all_eq)
            gen->enums.has_eq[idx] = 1;
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

// Emit implementations for all instantiated generic type methods
static void emit_generic_method_impls(CodeGen* gen, Node* ast) {
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

            // Check if function is void
            int is_void = return_type_is_void(fdn->return_type);

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
            emit(gen, ") {\n");

            // Clear defer stack for this function
            defer_clear(gen);
            gen->defer.return_type     = fdn->return_type;
            gen->generics.tmpl         = is_struct ? template : NULL;
            gen->generics.modules      = fdn->accessible_modules;
            gen->generics.module_count = fdn->accessible_modules_count;

            // Track if we're inside an enum method body (for match(self) dereference)
            int was_in_enum_method = gen->in_enum_method;
            if (info->type->kind == TYPE_ENUM) {
                gen->in_enum_method = 1;
            }

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
            gen->out.indent++;

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

            gen->in_enum_method = was_in_enum_method;

            // Emit any remaining defers at function end (for void functions or fallthrough)
            if (has_defers) {
                for (int d = gen->defer.count - 1; d >= 0; d--) {
                    emit_stmt(gen, gen->defer.stack[d]);
                }
            }

            gen->out.indent--;
            emit(gen, "}\n\n");

            // Clear RC tracking for generic method
            rc_clear_all(gen);

            gen->generics.subst        = NULL;
            gen->generics.tmpl         = NULL;
            gen->generics.modules      = NULL;
            gen->generics.module_count = 0;
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

    int has_user_main = program_has_user_main(ast);

    collect_types_and_aliases(gen, ast);
    emit_c_headers(gen);
    emit_rc_runtime(gen);
    emit_bounds_checks(gen);
    emit_string_helpers(gen);
    emit_tuple_typedefs(gen);
    register_enum_names(gen, ast);
    compute_enum_rc_flags(gen, ast);
    compute_enum_eq_flags(gen, ast);
    emit_struct_forward_decls(gen, ast);
    emit_vec_typedefs(gen);
    emit_span_typedefs(gen);
    emit_enum_typedefs(gen, ast);
    emit_generic_enum_typedefs(gen, ast);
    emit_enum_rc_helpers(gen, ast);
    emit_struct_body_typedefs(gen, ast);
    emit_function_forward_decls(gen, ast);
    emit_enum_eq_helpers(gen, ast);
    emit_struct_rc_dec_forward_decls(gen, ast);
    emit_vec_rc_dec(gen);
    emit_vec_methods(gen);
    emit_struct_rc_dec(gen, ast);
    emit_declarations(gen, ast);
    emit_generic_method_impls(gen, ast);
    if (gen->test_mode) {
        emit_test_runner(gen, ast);
    } else if (has_user_main) {
        emit_main_wrapper(gen);
    }
}
