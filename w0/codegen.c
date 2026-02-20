#include "codegen.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_emit.h"
#include "codegen_internal.h"
#include "codegen_types.h"
#include "sem_info.h"
#include "types.h"
#include "vec.h"

// Register a tuple type and return its index (for typedef name)
static int register_tuple_type(CodeGen* gen, Type* type) {
    // Check if already registered
    for (int i = 0; i < gen->tuple_type_count; i++) {
        if (type_equals(gen->tuple_types[i], type))
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
    case NODE_TEST_DECL:
        if (decl->as.test_decl.body)
            collect_tuple_types_from_stmt(gen, decl->as.test_decl.body);
        break;
    default:
        break;
    }
}

// Initialize the code generator with output stream, checker results, and RC debug flag
void codegen_init(CodeGen* gen, FILE* out, CodeGenChecker checker_data, int rc_debug, int test_mode,
                  const char* source_file, int line_directives) {
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

    gen->string_lits.values   = NULL;
    gen->string_lits.lengths  = NULL;
    gen->string_lits.count    = 0;
    gen->string_lits.capacity = 0;

    gen->hoist.nodes    = NULL;
    gen->hoist.names    = NULL;
    gen->hoist.count    = 0;
    gen->hoist.capacity = 0;

    gen->lambdas.nodes    = NULL;
    gen->lambdas.count    = 0;
    gen->lambdas.capacity = 0;

    gen->thunks.c_names    = NULL;
    gen->thunks.func_types = NULL;
    gen->thunks.count      = 0;
    gen->thunks.capacity   = 0;

    gen->capture_ctx.names = NULL;
    gen->capture_ctx.count = 0;

    gen->tuple_types         = NULL;
    gen->tuple_type_count    = 0;
    gen->tuple_type_capacity = 0;
    gen->current_module      = NULL;
    gen->in_enum_method      = 0;
    gen->test_mode           = test_mode;
    gen->test_index          = 0;
    gen->source_file         = source_file;
    gen->line_directives     = line_directives;
}

void codegen_free(CodeGen* gen) {
    free(gen->defer.stack);
    for (int i = 0; i < gen->rc.count; i++) {
        free(gen->rc.vars[i].name);
    }
    free(gen->rc.vars);
    for (int i = 0; i < gen->string_lits.count; i++) {
        free(gen->string_lits.values[i]);
    }
    free(gen->string_lits.values);
    free(gen->string_lits.lengths);
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
    for (int i = 0; i < gen->hoist.count; i++) {
        free(gen->hoist.names[i]);
    }
    free(gen->hoist.nodes);
    free(gen->hoist.names);
    free(gen->lambdas.nodes);
    for (int i = 0; i < gen->thunks.count; i++) {
        free(gen->thunks.c_names[i]);
    }
    free(gen->thunks.c_names);
    free(gen->thunks.func_types);
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

// Register a string literal in the table, deduplicating by value. Returns the index.
static int register_string_lit(CodeGen* gen, const char* value, int length) {
    for (int i = 0; i < gen->string_lits.count; i++) {
        if (gen->string_lits.lengths[i] == length &&
            memcmp(gen->string_lits.values[i], value, length) == 0)
            return i;
    }
    VEC_GROW(gen->string_lits.values, gen->string_lits.count, gen->string_lits.capacity);
    gen->string_lits.lengths =
        xrealloc(gen->string_lits.lengths, gen->string_lits.capacity * sizeof(int));
    char* copy = xmalloc(length + 1);
    memcpy(copy, value, length);
    copy[length]                                     = '\0';
    gen->string_lits.values[gen->string_lits.count]  = copy;
    gen->string_lits.lengths[gen->string_lits.count] = length;
    return gen->string_lits.count++;
}

// Recursively walk AST to collect all string literals
static void collect_string_literals_node(CodeGen* gen, Node* node);

static void collect_string_literals_nodelist(CodeGen* gen, NodeList* list) {
    if (!list)
        return;
    for (int i = 0; i < list->count; i++) {
        collect_string_literals_node(gen, list->nodes[i]);
    }
}

static void collect_string_interp_literal(CodeGen* gen, Node* node) {
    // Check if all parts are text — if so, register the concatenation
    int all_text = 1;
    for (int i = 0; i < node->as.string_interp.part_count; i++) {
        if (node->as.string_interp.parts.nodes[i]->type != NODE_STRING_LIT) {
            all_text = 0;
            break;
        }
    }
    if (all_text && node->as.string_interp.part_count > 0) {
        int total = 0;
        for (int i = 0; i < node->as.string_interp.part_count; i++) {
            total += node->as.string_interp.parts.nodes[i]->as.string_lit.length;
        }
        char* concat = xmalloc(total + 1);
        int   pos    = 0;
        for (int i = 0; i < node->as.string_interp.part_count; i++) {
            Node* p = node->as.string_interp.parts.nodes[i];
            memcpy(concat + pos, p->as.string_lit.value, p->as.string_lit.length);
            pos += p->as.string_lit.length;
        }
        concat[total] = '\0';
        register_string_lit(gen, concat, total);
        free(concat);
    }
    collect_string_literals_nodelist(gen, &node->as.string_interp.parts);
}

static int collect_string_literals_expr_node(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_STRING_LIT:
        register_string_lit(gen, node->as.string_lit.value, node->as.string_lit.length);
        return 1;
    case NODE_STRING_INTERP:
        collect_string_interp_literal(gen, node);
        return 1;
    case NODE_BINARY:
        collect_string_literals_node(gen, node->as.binary.left);
        collect_string_literals_node(gen, node->as.binary.right);
        return 1;
    case NODE_UNARY:
        collect_string_literals_node(gen, node->as.unary.operand);
        return 1;
    case NODE_CALL:
        collect_string_literals_node(gen, node->as.call.func);
        collect_string_literals_nodelist(gen, &node->as.call.args);
        return 1;
    case NODE_MEMBER:
        collect_string_literals_node(gen, node->as.member.object);
        return 1;
    case NODE_INDEX:
        collect_string_literals_node(gen, node->as.index.object);
        collect_string_literals_node(gen, node->as.index.index);
        return 1;
    case NODE_SLICE:
        collect_string_literals_node(gen, node->as.slice.object);
        collect_string_literals_node(gen, node->as.slice.start);
        collect_string_literals_node(gen, node->as.slice.end);
        return 1;
    case NODE_ASSIGN:
        collect_string_literals_node(gen, node->as.assign.target);
        collect_string_literals_node(gen, node->as.assign.value);
        return 1;
    case NODE_CAST:
        collect_string_literals_node(gen, node->as.cast_expr.expr);
        return 1;
    case NODE_NEW_EXPR:
        collect_string_literals_node(gen, node->as.new_expr.init);
        collect_string_literals_nodelist(gen, &node->as.new_expr.args);
        return 1;
    case NODE_STRUCT_INIT:
        collect_string_literals_nodelist(gen, &node->as.struct_init.fields);
        return 1;
    case NODE_FIELD_INIT:
        collect_string_literals_node(gen, node->as.field_init.value);
        return 1;
    case NODE_ENUM_VALUE:
        collect_string_literals_nodelist(gen, &node->as.enum_value.args);
        return 1;
    case NODE_TUPLE_LIT:
        collect_string_literals_nodelist(gen, &node->as.tuple_lit.elements);
        return 1;
    case NODE_LAMBDA:
        collect_string_literals_nodelist(gen, &node->as.lambda.params);
        collect_string_literals_node(gen, node->as.lambda.body);
        return 1;
    default:
        return 0;
    }
}

static int collect_string_literals_stmt_node(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_BLOCK:
        collect_string_literals_nodelist(gen, &node->as.block.stmts);
        return 1;
    case NODE_VAR_DECL:
        collect_string_literals_node(gen, node->as.var_decl.init);
        return 1;
    case NODE_RETURN:
        collect_string_literals_node(gen, node->as.return_stmt.value);
        return 1;
    case NODE_IF:
        collect_string_literals_node(gen, node->as.if_stmt.cond);
        collect_string_literals_node(gen, node->as.if_stmt.then_block);
        collect_string_literals_node(gen, node->as.if_stmt.else_block);
        return 1;
    case NODE_FOR:
        collect_string_literals_node(gen, node->as.for_stmt.init);
        collect_string_literals_node(gen, node->as.for_stmt.cond);
        collect_string_literals_node(gen, node->as.for_stmt.post);
        collect_string_literals_node(gen, node->as.for_stmt.body);
        return 1;
    case NODE_FOREACH:
        collect_string_literals_node(gen, node->as.foreach_stmt.collection);
        collect_string_literals_node(gen, node->as.foreach_stmt.start);
        collect_string_literals_node(gen, node->as.foreach_stmt.end);
        collect_string_literals_node(gen, node->as.foreach_stmt.body);
        return 1;
    case NODE_WHILE:
        collect_string_literals_node(gen, node->as.while_stmt.cond);
        collect_string_literals_node(gen, node->as.while_stmt.body);
        return 1;
    case NODE_EXPR_STMT:
        collect_string_literals_node(gen, node->as.expr_stmt.expr);
        return 1;
    case NODE_MATCH:
        collect_string_literals_node(gen, node->as.match_stmt.expr);
        collect_string_literals_nodelist(gen, &node->as.match_stmt.arms);
        return 1;
    case NODE_MATCH_ARM:
        collect_string_literals_node(gen, node->as.match_arm.pattern_expr);
        collect_string_literals_node(gen, node->as.match_arm.body);
        return 1;
    case NODE_DEFER:
        collect_string_literals_node(gen, node->as.defer_stmt.stmt);
        return 1;
    default:
        return 0;
    }
}

static int collect_string_literals_decl_node(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_PROGRAM:
        collect_string_literals_nodelist(gen, &node->as.program.modules);
        return 1;
    case NODE_MODULE:
        collect_string_literals_nodelist(gen, &node->as.module.decls);
        return 1;
    case NODE_FUNC_DECL:
        collect_string_literals_node(gen, node->as.func_decl.body);
        return 1;
    case NODE_IMPL_DECL:
        collect_string_literals_nodelist(gen, &node->as.impl_decl.methods);
        return 1;
    case NODE_TEST_DECL:
        collect_string_literals_node(gen, node->as.test_decl.body);
        return 1;
    default:
        return 0;
    }
}

static void collect_string_literals_node(CodeGen* gen, Node* node) {
    if (!node)
        return;
    if (collect_string_literals_expr_node(gen, node) ||
        collect_string_literals_stmt_node(gen, node) ||
        collect_string_literals_decl_node(gen, node)) {
        return;
    }
    switch (node->type) {
    case NODE_STRUCT_DECL:
    case NODE_ENUM_DECL:
    case NODE_TRAIT_DECL:
    case NODE_TYPE_ALIAS:
    case NODE_EXTERN_MODULE:
    case NODE_USE_DECL:
        break;
    default:
        break;
    }
}

static void collect_string_literals(CodeGen* gen, Node* ast) {
    collect_string_literals_node(gen, ast);
}

// ============================================================================
// Lambda collection and emission
// ============================================================================

static void collect_lambdas_node(CodeGen* gen, Node* node);

static void collect_lambdas_nodelist(CodeGen* gen, NodeList* list) {
    for (int i = 0; i < list->count; i++) {
        collect_lambdas_node(gen, list->nodes[i]);
    }
}

static void collect_lambdas_register_func_ref_thunk(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_IDENT:
        if (node->as.ident.resolved_func_type) {
            char c_name[256];
            snprintf(c_name, sizeof(c_name), "%.*s", node->as.ident.length, node->as.ident.name);
            register_thunk(gen, c_name, node->as.ident.resolved_func_type);
        }
        break;
    case NODE_MEMBER:
        if (node->as.member.is_method_ref && node->as.member.resolved_func_type) {
            const char* sname = sem_info_get_member_struct_name(gen->checker.sem, node,
                                                                node->as.member.struct_name);
            if (sname) {
                char c_name[256];
                snprintf(c_name, sizeof(c_name), "%s_%.*s", sname, node->as.member.length,
                         node->as.member.name);
                register_thunk(gen, c_name, node->as.member.resolved_func_type);
            }
        }
        break;
    default:
        break;
    }
}

static int collect_lambdas_expr_node(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_LAMBDA:
        VEC_GROW(gen->lambdas.nodes, gen->lambdas.count, gen->lambdas.capacity);
        gen->lambdas.nodes[gen->lambdas.count++] = node;
        collect_lambdas_node(gen, node->as.lambda.body);
        return 1;
    case NODE_BINARY:
        collect_lambdas_node(gen, node->as.binary.left);
        collect_lambdas_node(gen, node->as.binary.right);
        return 1;
    case NODE_UNARY:
        collect_lambdas_node(gen, node->as.unary.operand);
        return 1;
    case NODE_CALL:
        collect_lambdas_node(gen, node->as.call.func);
        collect_lambdas_nodelist(gen, &node->as.call.args);
        return 1;
    case NODE_MEMBER:
        collect_lambdas_node(gen, node->as.member.object);
        return 1;
    case NODE_INDEX:
        collect_lambdas_node(gen, node->as.index.object);
        collect_lambdas_node(gen, node->as.index.index);
        return 1;
    case NODE_SLICE:
        collect_lambdas_node(gen, node->as.slice.object);
        collect_lambdas_node(gen, node->as.slice.start);
        collect_lambdas_node(gen, node->as.slice.end);
        return 1;
    case NODE_ASSIGN:
        collect_lambdas_node(gen, node->as.assign.target);
        collect_lambdas_node(gen, node->as.assign.value);
        return 1;
    case NODE_CAST:
        collect_lambdas_node(gen, node->as.cast_expr.expr);
        return 1;
    case NODE_TRY_EXPR:
        collect_lambdas_node(gen, node->as.try_expr.expr);
        return 1;
    case NODE_NEW_EXPR:
        collect_lambdas_node(gen, node->as.new_expr.init);
        collect_lambdas_nodelist(gen, &node->as.new_expr.args);
        return 1;
    case NODE_STRUCT_INIT:
        collect_lambdas_nodelist(gen, &node->as.struct_init.fields);
        return 1;
    case NODE_FIELD_INIT:
        collect_lambdas_node(gen, node->as.field_init.value);
        return 1;
    case NODE_ENUM_VALUE:
        collect_lambdas_nodelist(gen, &node->as.enum_value.args);
        return 1;
    case NODE_TUPLE_LIT:
        collect_lambdas_nodelist(gen, &node->as.tuple_lit.elements);
        return 1;
    case NODE_ARRAY_LIT:
        collect_lambdas_nodelist(gen, &node->as.array_lit.elements);
        return 1;
    case NODE_STRING_INTERP:
        collect_lambdas_nodelist(gen, &node->as.string_interp.parts);
        return 1;
    default:
        return 0;
    }
}

static int collect_lambdas_stmt_node(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_BLOCK:
        collect_lambdas_nodelist(gen, &node->as.block.stmts);
        return 1;
    case NODE_VAR_DECL:
        collect_lambdas_node(gen, node->as.var_decl.init);
        return 1;
    case NODE_RETURN:
        collect_lambdas_node(gen, node->as.return_stmt.value);
        return 1;
    case NODE_IF:
        collect_lambdas_node(gen, node->as.if_stmt.cond);
        collect_lambdas_node(gen, node->as.if_stmt.then_block);
        collect_lambdas_node(gen, node->as.if_stmt.else_block);
        return 1;
    case NODE_FOR:
        collect_lambdas_node(gen, node->as.for_stmt.init);
        collect_lambdas_node(gen, node->as.for_stmt.cond);
        collect_lambdas_node(gen, node->as.for_stmt.post);
        collect_lambdas_node(gen, node->as.for_stmt.body);
        return 1;
    case NODE_FOREACH:
        collect_lambdas_node(gen, node->as.foreach_stmt.collection);
        collect_lambdas_node(gen, node->as.foreach_stmt.start);
        collect_lambdas_node(gen, node->as.foreach_stmt.end);
        collect_lambdas_node(gen, node->as.foreach_stmt.body);
        return 1;
    case NODE_WHILE:
        collect_lambdas_node(gen, node->as.while_stmt.cond);
        collect_lambdas_node(gen, node->as.while_stmt.body);
        return 1;
    case NODE_EXPR_STMT:
        collect_lambdas_node(gen, node->as.expr_stmt.expr);
        return 1;
    case NODE_MATCH:
        collect_lambdas_node(gen, node->as.match_stmt.expr);
        collect_lambdas_nodelist(gen, &node->as.match_stmt.arms);
        return 1;
    case NODE_MATCH_ARM:
        collect_lambdas_node(gen, node->as.match_arm.pattern_expr);
        collect_lambdas_node(gen, node->as.match_arm.body);
        return 1;
    case NODE_DEFER:
        collect_lambdas_node(gen, node->as.defer_stmt.stmt);
        return 1;
    default:
        return 0;
    }
}

static int collect_lambdas_decl_node(CodeGen* gen, Node* node) {
    switch (node->type) {
    case NODE_PROGRAM:
        collect_lambdas_nodelist(gen, &node->as.program.modules);
        return 1;
    case NODE_MODULE:
        collect_lambdas_nodelist(gen, &node->as.module.decls);
        return 1;
    case NODE_FUNC_DECL:
        collect_lambdas_node(gen, node->as.func_decl.body);
        return 1;
    case NODE_IMPL_DECL:
        collect_lambdas_nodelist(gen, &node->as.impl_decl.methods);
        return 1;
    case NODE_TEST_DECL:
        collect_lambdas_node(gen, node->as.test_decl.body);
        return 1;
    default:
        return 0;
    }
}

static void collect_lambdas_node(CodeGen* gen, Node* node) {
    if (!node)
        return;

    collect_lambdas_register_func_ref_thunk(gen, node);

    if (collect_lambdas_expr_node(gen, node) || collect_lambdas_stmt_node(gen, node) ||
        collect_lambdas_decl_node(gen, node)) {
        return;
    }
}

static void collect_lambdas(CodeGen* gen, Node* ast) {
    collect_lambdas_node(gen, ast);
}

// Emit environment struct typedefs and cleanup functions for capturing lambdas.
static void emit_lambda_env_typedefs(CodeGen* gen) {
    int emitted = 0;
    for (int i = 0; i < gen->lambdas.count; i++) {
        Node* lam = gen->lambdas.nodes[i];
        if (lam->as.lambda.captures.count == 0)
            continue;

        int id = lam->as.lambda.lambda_id;

        // Emit env struct typedef
        emit(gen, "typedef struct {\n");
        for (int c = 0; c < lam->as.lambda.captures.count; c++) {
            emit(gen, "    ");
            emit_resolved_type(gen, lam->as.lambda.captures.types[c]);
            emit(gen, " %s;\n", lam->as.lambda.captures.names[c]);
        }
        emit(gen, "} __lambda_%d_env;\n", id);

        // Emit cleanup function (only if there are RC captures)
        int has_rc = 0;
        for (int c = 0; c < lam->as.lambda.captures.count; c++) {
            if (lam->as.lambda.captures.is_rc[c]) {
                has_rc = 1;
                break;
            }
        }
        if (has_rc) {
            emit(gen, "static void __lambda_%d_env_cleanup(void* __raw) {\n", id);
            emit(gen, "    __lambda_%d_env* __e = (__lambda_%d_env*)__raw;\n", id, id);
            for (int c = 0; c < lam->as.lambda.captures.count; c++) {
                if (lam->as.lambda.captures.is_rc[c]) {
                    Type* ct = lam->as.lambda.captures.types[c];
                    if (ct && ct->kind == TYPE_STRING) {
                        emit(gen, "    __rc_dec((void*)__e->%s);\n",
                             lam->as.lambda.captures.names[c]);
                    } else if (ct && ct->kind == TYPE_FUNC) {
                        emit(gen, "    __rc_dec(__e->%s.env);\n", lam->as.lambda.captures.names[c]);
                    } else {
                        emit(gen, "    __rc_dec(__e->%s);\n", lam->as.lambda.captures.names[c]);
                    }
                }
            }
            emit(gen, "}\n");
        }
        emit(gen, "\n");
        emitted = 1;
    }
    if (emitted)
        emit(gen, "\n");
}

static void emit_lambda_forward_decls(CodeGen* gen) {
    for (int i = 0; i < gen->lambdas.count; i++) {
        Node* lam       = gen->lambdas.nodes[i];
        Type* func_type = lam->as.lambda.resolved_type;
        if (!func_type)
            continue;

        emit(gen, "static ");
        emit_resolved_type(gen, func_type->as.func.return_type);
        emit(gen, " __lambda_%d(void* __env", lam->as.lambda.lambda_id);
        for (int p = 0; p < func_type->as.func.param_count; p++) {
            emit(gen, ", ");
            emit_resolved_type(gen, func_type->as.func.param_types[p]);
            emit(gen, " %s", lam->as.lambda.params.nodes[p]->as.param.name);
        }
        emit(gen, ");\n");
    }
    if (gen->lambdas.count > 0)
        emit(gen, "\n");
}

static void emit_lambda_definitions(CodeGen* gen) {
    for (int i = 0; i < gen->lambdas.count; i++) {
        Node* lam       = gen->lambdas.nodes[i];
        Type* func_type = lam->as.lambda.resolved_type;
        if (!func_type)
            continue;

        emit(gen, "static ");
        emit_resolved_type(gen, func_type->as.func.return_type);
        emit(gen, " __lambda_%d(void* __env", lam->as.lambda.lambda_id);
        for (int p = 0; p < func_type->as.func.param_count; p++) {
            emit(gen, ", ");
            emit_resolved_type(gen, func_type->as.func.param_types[p]);
            emit(gen, " %s", lam->as.lambda.params.nodes[p]->as.param.name);
        }
        emit(gen, ") {\n");

        gen->out.indent++;
        int has_captures = lam->as.lambda.captures.count > 0;
        if (has_captures) {
            int id = lam->as.lambda.lambda_id;
            emit_indent(gen);
            emit(gen, "__lambda_%d_env* __cenv = (__lambda_%d_env*)__env;\n", id, id);
            // Set capture context for ident substitution
            gen->capture_ctx.names = lam->as.lambda.captures.names;
            gen->capture_ctx.count = lam->as.lambda.captures.count;
        } else {
            emit_indent(gen);
            emit(gen, "(void)__env;\n");
        }
        if (lam->as.lambda.is_expr_body) {
            // Expression body: return expr;
            Node* body = lam->as.lambda.body;
            // Check for intermediate owned temps (e.g., nested string concat).
            // Temporarily clear the outermost is_owned_temp — its ownership
            // transfers to the caller, so it must not be hoisted/dec'd.
            int ret_is_owned = body->is_owned_temp;
            if (ret_is_owned)
                body->is_owned_temp = 0;
            int body_has_temps = has_owned_temps(body);
            if (body_has_temps) {
                int saved = hoist_owned_temps(gen, body);
                if (ret_is_owned)
                    body->is_owned_temp = 1; // restore before emit_expr
                int tmp = gen->out.temp_count++;
                emit_indent(gen);
                emit(gen, "typeof(");
                emit_expr(gen, body);
                emit(gen, ") __rc_ret%d = ", tmp);
                emit_expr(gen, body);
                emit(gen, ";\n");
                cleanup_owned_temps(gen, saved);
                emit_indent(gen);
                emit(gen, "return __rc_ret%d;\n", tmp);
            } else {
                if (ret_is_owned)
                    body->is_owned_temp = 1; // restore
                emit_indent(gen);
                emit(gen, "return ");
                emit_expr(gen, body);
                emit(gen, ";\n");
            }
        } else {
            // Block body
            emit_block_contents(gen, lam->as.lambda.body);
            // Implicit void return cleanup
            if (func_type->as.func.return_type == type_void) {
                rc_cleanup_all(gen, NULL);
            }
        }
        // Clear capture context
        if (has_captures) {
            gen->capture_ctx.names = NULL;
            gen->capture_ctx.count = 0;
        }
        gen->out.indent--;
        emit(gen, "}\n\n");
        rc_clear_all(gen);
    }
}

// Register a thunk for a named function used as a closure value.
// Deduplicates by C function name.
void register_thunk(CodeGen* gen, const char* c_name, Type* func_type) {
    // Check if already registered
    for (int i = 0; i < gen->thunks.count; i++) {
        if (strcmp(gen->thunks.c_names[i], c_name) == 0) {
            return;
        }
    }
    VEC_GROW(gen->thunks.c_names, gen->thunks.count, gen->thunks.capacity);
    gen->thunks.func_types = xrealloc(gen->thunks.func_types, gen->thunks.capacity * sizeof(Type*));
    gen->thunks.c_names[gen->thunks.count]    = xstrdup(c_name);
    gen->thunks.func_types[gen->thunks.count] = func_type;
    gen->thunks.count++;
}

static void emit_thunk_forward_decls(CodeGen* gen) {
    for (int i = 0; i < gen->thunks.count; i++) {
        Type* ft = gen->thunks.func_types[i];
        emit(gen, "static ");
        emit_resolved_type(gen, ft->as.func.return_type);
        emit(gen, " __%s_thunk(void* __env", gen->thunks.c_names[i]);
        for (int p = 0; p < ft->as.func.param_count; p++) {
            emit(gen, ", ");
            emit_resolved_type(gen, ft->as.func.param_types[p]);
            emit(gen, " __p%d", p);
        }
        emit(gen, ");\n");
    }
    if (gen->thunks.count > 0)
        emit(gen, "\n");
}

static void emit_thunk_definitions(CodeGen* gen) {
    for (int i = 0; i < gen->thunks.count; i++) {
        Type* ft = gen->thunks.func_types[i];
        emit(gen, "static ");
        emit_resolved_type(gen, ft->as.func.return_type);
        emit(gen, " __%s_thunk(void* __env", gen->thunks.c_names[i]);
        for (int p = 0; p < ft->as.func.param_count; p++) {
            emit(gen, ", ");
            emit_resolved_type(gen, ft->as.func.param_types[p]);
            emit(gen, " __p%d", p);
        }
        emit(gen, ") {\n");
        emit(gen, "    (void)__env;\n");
        emit(gen, "    ");
        if (ft->as.func.return_type && ft->as.func.return_type != type_void) {
            emit(gen, "return ");
        }
        emit(gen, "%s(", gen->thunks.c_names[i]);
        for (int p = 0; p < ft->as.func.param_count; p++) {
            if (p > 0)
                emit(gen, ", ");
            emit(gen, "__p%d", p);
        }
        emit(gen, ");\n");
        emit(gen, "}\n\n");
    }
}

// Emit escaped string content (shared helper for string literal table)
static void emit_escaped_string(CodeGen* gen, const char* value, int length) {
    for (int i = 0; i < length; i++) {
        char c = value[i];
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
        case '\0':
            emit(gen, "\\0");
            break;
        default:
            emit(gen, "%c", c);
            break;
        }
    }
}

// Emit static RC string literal structs with immortal (SIZE_MAX) refcount
static void emit_string_literals(CodeGen* gen) {
    if (gen->string_lits.count == 0)
        return;
    for (int i = 0; i < gen->string_lits.count; i++) {
        emit(gen,
             "static struct { __RcHeader hdr; char data[%d]; } __rc_str_%d = "
             "{ {SIZE_MAX, NULL}, \"",
             gen->string_lits.lengths[i] + 1, i);
        emit_escaped_string(gen, gen->string_lits.values[i], gen->string_lits.lengths[i]);
        emit(gen, "\" };\n");
    }
    emit(gen, "\n");
}

// Look up a string literal's index in the table. Returns -1 if not found.
int lookup_string_lit(CodeGen* gen, const char* value, int length) {
    for (int i = 0; i < gen->string_lits.count; i++) {
        if (gen->string_lits.lengths[i] == length &&
            memcmp(gen->string_lits.values[i], value, length) == 0)
            return i;
    }
    return -1;
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

// Return whether any variant in a declared enum carries data fields.
static int enum_decl_has_data(Node* decl) {
    for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
        if (decl->as.enum_decl.values.nodes[v]->as.enum_variant.types.count > 0) {
            return 1;
        }
    }
    return 0;
}

// Return whether a struct-typed field node resolves to a struct with Eq implementation.
static int struct_field_type_node_has_eq(CodeGen* gen, Node* tnode) {
    if (!is_struct_type(gen, tnode)) {
        return 1;
    }

    Node*       resolved = resolve_alias(gen, tnode);
    const char* sname    = NULL;
    int         owned    = 0;
    if (resolved->type == NODE_IDENT) {
        sname = resolved->as.ident.name;
    } else if (resolved->type == NODE_GENERIC_TYPE) {
        sname = build_mangled_name_from_generic_node(gen, resolved);
        owned = 1;
    }

    int has_eq = sname && struct_has_eq_impl(gen, sname);
    if (owned) {
        free((char*)sname);
    }
    return has_eq;
}

// Return whether all struct-typed fields in a non-generic enum declaration are Eq-comparable.
static int enum_decl_all_struct_fields_have_eq(CodeGen* gen, Node* decl) {
    for (int v = 0; v < decl->as.enum_decl.values.count; v++) {
        Node* var = decl->as.enum_decl.values.nodes[v];
        for (int t = 0; t < var->as.enum_variant.types.count; t++) {
            if (!struct_field_type_node_has_eq(gen, var->as.enum_variant.types.nodes[t])) {
                return 0;
            }
        }
    }
    return 1;
}

// Return whether all struct payload fields in a generic enum instance are Eq-comparable.
static int enum_instance_all_struct_fields_have_eq(GenericInstance* inst) {
    for (int v = 0; v < inst->type->as.enm.value_count; v++) {
        for (int f = 0; f < inst->type->as.enm.variant_type_counts[v]; f++) {
            Type* ft = inst->type->as.enm.variant_types[v][f];
            if (ft->kind == TYPE_STRUCT && !ft->as.struc.has_eq) {
                return 0;
            }
        }
    }
    return 1;
}

// Compute which data enums support equality (all struct-typed variant fields have Eq)
static void compute_enum_eq_flags(CodeGen* gen, Node* ast) {
    if (gen->enums.count == 0) {
        return;
    }
    gen->enums.has_eq = xcalloc(gen->enums.count, sizeof(int));

    // Non-generic data enums
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE) {
            continue;
        }
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL || decl->as.enum_decl.type_param_count > 0) {
                continue;
            }

            int idx = enum_index(gen, decl->as.enum_decl.name);
            if (idx < 0 || !enum_decl_has_data(decl)) {
                continue;
            }

            if (enum_decl_all_struct_fields_have_eq(gen, decl)) {
                gen->enums.has_eq[idx] = 1;
            }
        }
    }

    // Generic data enum instances: use checker Type info
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* inst = &gen->checker.instances[gi];
        if (inst->type->kind != TYPE_ENUM || !inst->type->as.enm.has_data) {
            continue;
        }

        int idx = enum_index(gen, inst->mangled_name);
        if (idx < 0) {
            continue;
        }

        if (enum_instance_all_struct_fields_have_eq(inst)) {
            gen->enums.has_eq[idx] = 1;
        }
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

typedef struct {
    Node* node;
    int   is_struct;
    int   param_count;
} GenericTypeTemplateInfo;

typedef struct {
    char**           method_params;
    Type**           method_args;
    int              method_bind_count;
    char**           combined_params;
    Type**           combined_args;
    TypeSubstContext subst_ctx;
} GenericMethodSubst;

static int resolve_generic_type_template(Node* ast, const char* base_name,
                                         GenericTypeTemplateInfo* out) {
    out->node        = find_generic_struct_decl(ast, base_name);
    out->is_struct   = 0;
    out->param_count = 0;

    if (out->node) {
        out->is_struct   = 1;
        out->param_count = out->node->as.struct_decl.type_param_count;
        return 1;
    }

    out->node = find_generic_enum_decl(ast, base_name);
    if (!out->node) {
        return 0;
    }

    out->param_count = out->node->as.enum_decl.type_param_count;
    return 1;
}

static int block_has_top_level_defer(Node* body) {
    if (!body || body->type != NODE_BLOCK) {
        return 0;
    }
    for (int i = 0; i < body->as.block.stmts.count; i++) {
        Node* stmt = body->as.block.stmts.nodes[i];
        if (stmt && stmt->type == NODE_DEFER) {
            return 1;
        }
    }
    return 0;
}

static void emit_block_stmts(CodeGen* gen, Node* body) {
    if (!body || body->type != NODE_BLOCK) {
        return;
    }
    for (int i = 0; i < body->as.block.stmts.count; i++) {
        emit_stmt(gen, body->as.block.stmts.nodes[i]);
    }
}

static Node* method_body_for_instance(GenericInstance* info, int method_index,
                                      Node* fallback_body) {
    if (info->method_bodies && method_index < info->method_body_count &&
        info->method_bodies[method_index]) {
        return info->method_bodies[method_index];
    }
    return fallback_body;
}

static void free_generic_method_subst(GenericMethodSubst* subst) {
    for (int i = 0; i < subst->method_bind_count; i++) {
        free(subst->method_params[i]);
    }
    free(subst->method_params);
    free(subst->method_args);
    free(subst->combined_params);
    free(subst->combined_args);
}

static void setup_generic_method_subst(CodeGen* gen, GenericInstance* info,
                                       GenericTypeTemplateInfo* tinfo, func_decl_node* fdn,
                                       GenericMethodSubst* subst) {
    memset(subst, 0, sizeof(*subst));

    codegen_extract_method_bindings(&fdn->receiver_type_args, info->type_args, info->type_arg_count,
                                    &subst->method_params, &subst->method_args,
                                    &subst->method_bind_count);

    int combined_count     = tinfo->param_count + subst->method_bind_count;
    subst->combined_params = xmalloc(combined_count * sizeof(char*));
    subst->combined_args   = xmalloc(combined_count * sizeof(Type*));

    for (int i = 0; i < tinfo->param_count; i++) {
        if (tinfo->is_struct) {
            subst->combined_params[i] = tinfo->node->as.struct_decl.type_params[i];
        } else {
            subst->combined_params[i] = tinfo->node->as.enum_decl.type_params[i];
        }
        subst->combined_args[i] = info->type_args[i];
    }
    for (int i = 0; i < subst->method_bind_count; i++) {
        int idx                     = tinfo->param_count + i;
        subst->combined_params[idx] = subst->method_params[i];
        subst->combined_args[idx]   = subst->method_args[i];
    }

    subst->subst_ctx.type_params = subst->combined_params;
    subst->subst_ctx.type_args   = subst->combined_args;
    subst->subst_ctx.count       = combined_count;
    gen->generics.subst          = &subst->subst_ctx;
}

static void emit_generic_method_signature(CodeGen* gen, GenericInstance* info,
                                          func_decl_node* fdn) {
    emit_func_return_type(gen, fdn);
    emit(gen, " %s_%s(", info->mangled_name, fdn->name);

    if (fdn->receiver_is_const) {
        emit(gen, "const ");
    }
    emit(gen, "%s* self", info->mangled_name);

    for (int i = 0; i < fdn->params.count; i++) {
        emit(gen, ", ");
        Node* param = fdn->params.nodes[i];
        if (param->as.param.is_const) {
            emit(gen, "const ");
        }
        emit_type_with_name(gen, param->as.param.type, param->as.param.name);
    }

    emit(gen, ") {\n");
}

static void emit_generic_method_impl(CodeGen* gen, GenericInstance* info,
                                     GenericTypeTemplateInfo* tinfo, Node* method,
                                     int method_index) {
    func_decl_node*    fdn = &method->as.func_decl;
    GenericMethodSubst subst;
    setup_generic_method_subst(gen, info, tinfo, fdn, &subst);

    int is_void = codegen_return_type_is_void(fdn->return_type);
    emit_generic_method_signature(gen, info, fdn);

    defer_clear(gen);
    gen->defer.return_type     = fdn->return_type;
    gen->generics.tmpl         = tinfo->is_struct ? tinfo->node : NULL;
    gen->generics.modules      = fdn->accessible_modules;
    gen->generics.module_count = fdn->accessible_modules_count;

    int was_in_enum_method = gen->in_enum_method;
    if (info->type->kind == TYPE_ENUM) {
        gen->in_enum_method = 1;
    }

    Node* method_body = method_body_for_instance(info, method_index, fdn->body);
    int   has_defers  = block_has_top_level_defer(method_body);

    gen->out.indent++;

    if (has_defers && !is_void) {
        emit_indent(gen);
        emit_type(gen, fdn->return_type);
        emit(gen, " __ret;\n");
    }

    emit_block_stmts(gen, method_body);

    gen->in_enum_method = was_in_enum_method;

    if (gen->rc.count > 0 && method_body && method_body->type == NODE_BLOCK) {
        int   sc   = method_body->as.block.stmts.count;
        Node* last = sc > 0 ? method_body->as.block.stmts.nodes[sc - 1] : NULL;
        if (!last || last->type != NODE_RETURN) {
            rc_cleanup_all(gen, NULL);
        }
    }

    if (has_defers) {
        for (int d = gen->defer.count - 1; d >= 0; d--) {
            emit_stmt(gen, gen->defer.stack[d]);
        }
    }

    gen->out.indent--;
    emit(gen, "}\n\n");

    rc_clear_all(gen);

    gen->generics.subst        = NULL;
    gen->generics.tmpl         = NULL;
    gen->generics.modules      = NULL;
    gen->generics.module_count = 0;
    free_generic_method_subst(&subst);
}

// Emit implementations for all instantiated generic type methods
static void emit_generic_method_impls(CodeGen* gen, Node* ast) {
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance*        info = &gen->checker.instances[i];
        GenericTypeTemplateInfo tinfo;
        Node**                  methods      = NULL;
        int                     method_count = 0;

        if (!resolve_generic_type_template(ast, info->base_name, &tinfo)) {
            continue;
        }

        collect_generic_methods(ast, info->base_name, &methods, &method_count);

        for (int j = 0; j < method_count; j++) {
            emit_generic_method_impl(gen, info, &tinfo, methods[j], j);
        }

        free(methods);
    }
}

GenericFuncDef* lookup_generic_func_def_for_instance(CodeGen* gen, const char* base_name) {
    for (int i = 0; i < gen->checker.func_def_count; i++) {
        GenericFuncDef* def = &gen->checker.func_defs[i];
        if (strcmp(def->name, base_name) == 0) {
            return def;
        }
    }
    return NULL;
}

// Parse a "Type.method" base_name to extract receiver_type and method_name.
// Returns 1 if found, 0 if not a method key.
int parse_method_key(const char* base_name, char* recv_out, int recv_size, char* method_out,
                     int method_size) {
    const char* dot = strchr(base_name, '.');
    if (!dot)
        return 0;
    int recv_len = (int)(dot - base_name);
    if (recv_len >= recv_size)
        recv_len = recv_size - 1;
    memcpy(recv_out, base_name, recv_len);
    recv_out[recv_len] = '\0';
    int method_len     = (int)strlen(dot + 1);
    if (method_len >= method_size)
        method_len = method_size - 1;
    memcpy(method_out, dot + 1, method_len);
    method_out[method_len] = '\0';
    return 1;
}

// Emit a method-level generic function implementation
static void emit_generic_method_func_impl(CodeGen* gen, Node* ast, GenericFuncInstance* inst) {
    char recv_name[128], method_name[128];
    if (!parse_method_key(inst->base_name, recv_name, sizeof(recv_name), method_name,
                          sizeof(method_name))) {
        return;
    }

    Node* tmpl = find_generic_method_func_decl(ast, recv_name, method_name);
    if (!tmpl)
        return;

    func_decl_node* fdn = &tmpl->as.func_decl;
    GenericFuncDef* def = lookup_generic_func_def_for_instance(gen, inst->base_name);
    if (!def || def->type_param_count != inst->type_arg_count) {
        return;
    }

    TypeSubstContext subst_ctx;
    subst_ctx.type_params = def->type_params;
    subst_ctx.type_args   = inst->type_args;
    subst_ctx.count       = def->type_param_count;
    gen->generics.subst   = &subst_ctx;

    int is_void = codegen_return_type_is_void(fdn->return_type);

    // Return type
    emit_func_return_type(gen, fdn);

    // Function name + self parameter
    emit(gen, " %s(", inst->mangled_name);
    if (fdn->receiver_is_const) {
        emit(gen, "const ");
    }
    emit_resolved_type(gen, inst->receiver_concrete);
    emit(gen, " self");

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
    gen->generics.modules      = fdn->accessible_modules;
    gen->generics.module_count = fdn->accessible_modules_count;

    int has_defers = block_has_top_level_defer(inst->body);

    gen->out.indent++;

    if (has_defers && !is_void) {
        emit_indent(gen);
        emit_type(gen, fdn->return_type);
        emit(gen, " __ret;\n");
    }

    emit_block_stmts(gen, inst->body);

    if (gen->rc.count > 0 && inst->body) {
        int   sc   = inst->body->as.block.stmts.count;
        Node* last = sc > 0 ? inst->body->as.block.stmts.nodes[sc - 1] : NULL;
        if (!last || last->type != NODE_RETURN) {
            rc_cleanup_all(gen, NULL);
        }
    }

    if (has_defers) {
        for (int d = gen->defer.count - 1; d >= 0; d--) {
            emit_stmt(gen, gen->defer.stack[d]);
        }
    }

    gen->out.indent--;
    emit(gen, "}\n\n");

    rc_clear_all(gen);

    gen->generics.subst        = NULL;
    gen->generics.modules      = NULL;
    gen->generics.module_count = 0;
}

// Emit implementations for all instantiated generic free functions
static void emit_generic_func_impls(CodeGen* gen, Node* ast) {
    for (int i = 0; i < gen->checker.func_instance_count; i++) {
        GenericFuncInstance* inst = &gen->checker.func_instances[i];

        // Method-level generics: use separate handler
        if (inst->is_method) {
            emit_generic_method_func_impl(gen, ast, inst);
            continue;
        }

        // Find template
        Node* tmpl = find_generic_func_decl(ast, inst->base_name);
        if (!tmpl)
            continue;

        func_decl_node* fdn = &tmpl->as.func_decl;

        // Set up type substitution context
        TypeSubstContext subst_ctx;
        subst_ctx.type_params = fdn->type_params;
        subst_ctx.type_args   = inst->type_args;
        subst_ctx.count       = inst->type_arg_count;
        gen->generics.subst   = &subst_ctx;

        int is_void = codegen_return_type_is_void(fdn->return_type);

        // Return type
        emit_func_return_type(gen, fdn);

        // Function name
        emit(gen, " %s(", inst->mangled_name);

        // Parameters
        if (fdn->params.count == 0) {
            emit(gen, "void");
        } else {
            for (int p = 0; p < fdn->params.count; p++) {
                if (p > 0)
                    emit(gen, ", ");
                Node* param = fdn->params.nodes[p];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
        }
        emit(gen, ") {\n");

        // Clear defer stack for this function
        defer_clear(gen);
        gen->defer.return_type     = fdn->return_type;
        gen->generics.modules      = fdn->accessible_modules;
        gen->generics.module_count = fdn->accessible_modules_count;

        // First pass: count defers to know if we need __ret
        int has_defers = block_has_top_level_defer(inst->body);

        // Body
        gen->out.indent++;

        // Declare __ret if function has defers and is non-void
        if (has_defers && !is_void) {
            emit_indent(gen);
            emit_type(gen, fdn->return_type);
            emit(gen, " __ret;\n");
        }

        // Emit function body
        emit_block_stmts(gen, inst->body);

        // RC cleanup for implicit void return
        if (gen->rc.count > 0 && inst->body) {
            int   sc   = inst->body->as.block.stmts.count;
            Node* last = sc > 0 ? inst->body->as.block.stmts.nodes[sc - 1] : NULL;
            if (!last || last->type != NODE_RETURN) {
                rc_cleanup_all(gen, NULL);
            }
        }

        // Emit any remaining defers at function end
        if (has_defers) {
            for (int d = gen->defer.count - 1; d >= 0; d--) {
                emit_stmt(gen, gen->defer.stack[d]);
            }
        }

        gen->out.indent--;
        emit(gen, "}\n\n");

        // Clear RC tracking for generic function
        rc_clear_all(gen);

        gen->generics.subst        = NULL;
        gen->generics.modules      = NULL;
        gen->generics.module_count = 0;
    }
}

// Main codegen entry point: emit all C code for a program AST
void codegen_emit(CodeGen* gen, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM)
        return;

    int has_user_main = program_has_user_main(ast);

    collect_types_and_aliases(gen, ast);
    collect_string_literals(gen, ast);
    emit_c_headers(gen);
    emit_rc_runtime(gen);
    emit_bounds_checks(gen);
    emit_string_helpers(gen);
    emit_string_literals(gen);
    emit_tuple_typedefs(gen);
    register_enum_names(gen, ast);
    compute_enum_rc_flags(gen, ast);
    compute_enum_eq_flags(gen, ast);
    emit_struct_forward_decls(gen, ast);
    emit_vec_typedefs(gen);
    emit_box_typedefs(gen);
    emit_span_typedefs(gen);
    emit_enum_typedefs(gen, ast);
    emit_generic_enum_typedefs(gen, ast);
    emit_enum_rc_helpers(gen, ast);
    emit_struct_body_typedefs(gen, ast);
    emit_function_forward_decls(gen, ast);
    collect_lambdas(gen, ast);
    emit_lambda_env_typedefs(gen);
    emit_lambda_forward_decls(gen);
    emit_thunk_forward_decls(gen);
    emit_enum_eq_helpers(gen, ast);
    emit_vec_cleanup(gen);
    emit_vec_methods(gen);
    emit_vec_user_methods(gen, ast);
    emit_struct_cleanup(gen, ast);
    emit_declarations(gen, ast);
    emit_lambda_definitions(gen);
    emit_thunk_definitions(gen);
    emit_generic_method_impls(gen, ast);
    emit_generic_func_impls(gen, ast);
    if (gen->test_mode) {
        emit_test_runner(gen, ast);
    } else if (has_user_main) {
        emit_main_wrapper(gen);
    }
}
