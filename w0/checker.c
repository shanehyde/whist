#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "checker_internal.h"
#include "vec.h"

#define SCOPE_SIZE 64

// Forward declarations for mutually recursive functions
static void check_decl(Checker* checker, Node* node);

// Forward declarations for statement checking helpers
static void check_var_decl_stmt(Checker* checker, Node* node);
static void check_for_stmt(Checker* checker, Node* node);
static void check_foreach_stmt(Checker* checker, Node* node);
static void check_return_stmt(Checker* checker, Node* node);

// Forward declarations for destructuring pattern checking
static int check_destruct_pattern_redefinitions(Checker* checker, DestructPattern* pattern,
                                                int line, int col);
static int check_destruct_pattern_redefinitions_internal(Checker* checker, DestructPattern* pattern,
                                                         int line, int col, char*** names,
                                                         int* count, int* capacity);
static int check_destruct_pattern_against_type(Checker* checker, DestructPattern* pattern,
                                               Type* type, int line, int col);
static void define_destruct_pattern_vars(Checker* checker, DestructPattern* pattern, Type* type,
                                         int is_const, int is_public);

// Forward declarations for check_decl helpers
static void check_extern_module_decl(Checker* checker, Node* node);
static void check_func_decl(Checker* checker, Node* node);
static void check_struct_decl(Checker* checker, Node* node);
static void check_enum_decl(Checker* checker, Node* node);
static void check_trait_decl(Checker* checker, Node* node);
static void check_type_alias_decl(Checker* checker, Node* node);
static void check_impl_decl(Checker* checker, Node* node);

// =============================================================================
// Utility functions
// =============================================================================

void check_error(Checker* checker, int line, int col, const char* fmt, ...) {
    checker->error_count++;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[line %d:%d] Error: ", line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

// Helper for type mismatch errors with consistent "expected X, got Y" format
void check_error_type(Checker* checker, int line, int col, const char* context, Type* expected,
                      Type* got) {
    check_error(checker, line, col, "%s: expected '%s', got '%s'", context, type_name(expected),
                type_name(got));
}

// Helper for "cannot do X to type Y" errors
void check_error_cannot(Checker* checker, int line, int col, const char* action, Type* type) {
    check_error(checker, line, col, "Cannot %s type '%s'", action, type_name(type));
}

static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int          c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

// =============================================================================
// Checker initialization and cleanup
// =============================================================================

void checker_init(Checker* checker) {
    checker->scope                            = NULL;
    checker->current_func_return              = NULL;
    checker->in_loop                          = 0;
    checker->error_count                      = 0;
    checker->error_msg[0]                     = '\0';
    checker->direct_imports                   = NULL;
    checker->direct_imports_count             = 0;
    checker->current_accessible_modules       = NULL;
    checker->current_accessible_modules_count = 0;
    checker->current_module                   = NULL;
    // Generic support
    checker->generic_defs              = NULL;
    checker->generic_def_count         = 0;
    checker->generic_def_capacity      = 0;
    checker->generic_instances         = NULL;
    checker->generic_instance_count    = 0;
    checker->generic_instance_capacity = 0;
    checker->current_type_params       = NULL;
    checker->current_type_args         = NULL;
    checker->current_type_param_count  = 0;
    // Span support
    checker->span_instances         = NULL;
    checker->span_instance_count    = 0;
    checker->span_instance_capacity = 0;
    // Vec support
    checker->vec_instances         = NULL;
    checker->vec_instance_count    = 0;
    checker->vec_instance_capacity = 0;
    // Trait support
    checker->trait_impls         = NULL;
    checker->trait_impl_count    = 0;
    checker->trait_impl_capacity = 0;
    checker->alias_depth         = 0;
    checker->enum_target_hint    = NULL;
    types_init();
}

void checker_set_direct_imports(Checker* checker, char** direct_imports, int count) {
    checker->direct_imports       = direct_imports;
    checker->direct_imports_count = count;
}

void checker_push_scope(Checker* checker) {
    Scope* scope   = xcalloc(1, sizeof(Scope));
    scope->symbols = xcalloc(SCOPE_SIZE, sizeof(Symbol*));
    scope->size    = SCOPE_SIZE;
    scope->parent  = checker->scope;
    checker->scope = scope;
}

void checker_pop_scope(Checker* checker) {
    Scope* scope = checker->scope;
    if (!scope)
        return;

    checker->scope = scope->parent;

    for (int i = 0; i < scope->size; i++) {
        Symbol* sym = scope->symbols[i];
        while (sym) {
            Symbol* next = sym->next;
            free(sym->name);
            free(sym->source_module);
            free(sym);
            sym = next;
        }
    }
    free(scope->symbols);
    free(scope);
}

void checker_free(Checker* checker) {
    while (checker->scope) {
        checker_pop_scope(checker);
    }
    // Free generic definitions
    for (int i = 0; i < checker->generic_def_count; i++) {
        free(checker->generic_defs[i].name);
        for (int j = 0; j < checker->generic_defs[i].type_param_count; j++) {
            free(checker->generic_defs[i].type_params[j]);
            free(checker->generic_defs[i].type_param_bounds[j]);
        }
        free(checker->generic_defs[i].type_params);
        free(checker->generic_defs[i].type_param_bounds);
        // Note: methods array contains pointers to AST nodes, don't free them
        free(checker->generic_defs[i].methods);
    }
    free(checker->generic_defs);
    // Free generic instances
    for (int i = 0; i < checker->generic_instance_count; i++) {
        free(checker->generic_instances[i].mangled_name);
        free(checker->generic_instances[i].base_name);
        free(checker->generic_instances[i].type_args);
    }
    free(checker->generic_instances);
    // Free span instances
    for (int i = 0; i < checker->span_instance_count; i++) {
        free(checker->span_instances[i].mangled_name);
    }
    free(checker->span_instances);
    // Free vec instances
    for (int i = 0; i < checker->vec_instance_count; i++) {
        free(checker->vec_instances[i].mangled_name);
    }
    free(checker->vec_instances);
    // Free trait implementations
    for (int i = 0; i < checker->trait_impl_count; i++) {
        free(checker->trait_impls[i].trait_name);
        free(checker->trait_impls[i].type_name);
    }
    free(checker->trait_impls);
    types_cleanup();
}

// =============================================================================
// Symbol table operations
// =============================================================================

Symbol* checker_define(Checker* checker, const char* name, SymbolKind kind, Type* type,
                       int is_const, int is_public, const char* source_module) {
    Scope*       scope = checker->scope;
    unsigned int index = hash_string(name) % scope->size;

    // Check for redefinition in current scope
    for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return NULL; // Already defined
        }
    }

    Symbol* sym           = xcalloc(1, sizeof(Symbol));
    sym->kind             = kind;
    sym->name             = xstrdup(name);
    sym->type             = type;
    sym->is_const         = is_const;
    sym->is_public        = is_public;
    sym->source_module    = source_module ? xstrdup(source_module) : NULL;
    sym->next             = scope->symbols[index];
    scope->symbols[index] = sym;
    return sym;
}

// Check if a module is accessible from the current context
static int is_module_accessible(Checker* checker, const char* source_module) {
    // NULL source_module means same module - always accessible
    if (!source_module) {
        return 1;
    }

    // If we're currently in the same module, it's accessible
    if (checker->current_module && strcmp(checker->current_module, source_module) == 0) {
        return 1;
    }

    // Use current function's accessible modules if set, otherwise use global direct_imports
    char** modules = checker->current_accessible_modules;
    int    count   = checker->current_accessible_modules_count;

    if (!modules) {
        modules = checker->direct_imports;
        count   = checker->direct_imports_count;
    }

    // Check if module is in the accessible list
    for (int i = 0; i < count; i++) {
        if (strcmp(modules[i], source_module) == 0) {
            return 1;
        }
    }

    return 0;
}

// Check if a name is a directly imported library module
int is_imported_module(Checker* checker, const char* name) {
    // Use current function's accessible modules if set, otherwise use global direct_imports
    char** modules = checker->current_accessible_modules;
    int    count   = checker->current_accessible_modules_count;

    if (!modules) {
        modules = checker->direct_imports;
        count   = checker->direct_imports_count;
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(modules[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Look up a symbol from a specific module (for module-qualified access)
Symbol* checker_lookup_in_module(Checker* checker, const char* module_name,
                                 const char* symbol_name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(symbol_name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, symbol_name) == 0 && sym->source_module &&
                strcmp(sym->source_module, module_name) == 0) {
                // Must be public for module-qualified access
                if (!sym->is_public) {
                    return NULL;
                }
                return sym;
            }
        }
    }
    return NULL;
}

Symbol* checker_lookup(Checker* checker, const char* name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                // Check module visibility
                if (!is_module_accessible(checker, sym->source_module)) {
                    continue; // Symbol exists but not accessible from this module
                }
                // Symbols from library imports require module qualification
                // Skip them here - they must be accessed via module.symbol syntax
                int same_module = (sym->source_module == NULL && checker->current_module == NULL) ||
                                  (sym->source_module && checker->current_module &&
                                   strcmp(sym->source_module, checker->current_module) == 0);
                if (sym->source_module && !same_module) {
                    continue; // Library symbol - require module qualification
                }
                return sym;
            }
        }
    }
    return NULL;
}

// Lookup without visibility checking (for error messages)
Symbol* checker_lookup_any(Checker* checker, const char* name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

Symbol* checker_lookup_local(Checker* checker, const char* name) {
    Scope* scope = checker->scope;
    if (!scope)
        return NULL;

    unsigned int index = hash_string(name) % scope->size;
    for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

// =============================================================================
// Destructuring pattern checking
// =============================================================================

// Check for redefinitions in a destructuring pattern (recursive)
// Returns 1 if error found, 0 otherwise
static int check_destruct_pattern_redefinitions(Checker* checker, DestructPattern* pattern,
                                                int line, int col) {
    int    name_count    = 0;
    int    name_capacity = 4;
    char** names         = xmalloc(name_capacity * sizeof(char*));

    int had_error = check_destruct_pattern_redefinitions_internal(
        checker, pattern, line, col, &names, &name_count, &name_capacity);

    for (int i = 0; i < name_count; i++) {
        free(names[i]);
    }
    free(names);

    return had_error;
}

static int check_destruct_pattern_redefinitions_internal(Checker* checker, DestructPattern* pattern,
                                                         int line, int col, char*** names,
                                                         int* count, int* capacity) {
    if (!pattern)
        return 0;

    switch (pattern->kind) {
    case PATTERN_IDENT: {
        const char* name = pattern->as.ident.name;

        if (checker_lookup_local(checker, name)) {
            check_error(checker, line, col, "Redefinition of '%s'", name);
            return 1;
        }

        for (int i = 0; i < *count; i++) {
            if (strcmp((*names)[i], name) == 0) {
                check_error(checker, line, col, "Redefinition of '%s'", name);
                return 1;
            }
        }

        if (*count >= *capacity) {
            *capacity *= 2;
            *names = xrealloc(*names, (*capacity) * sizeof(char*));
        }
        (*names)[*count] = xstrdup(name);
        (*count)++;
        return 0;
    }

    case PATTERN_TUPLE:
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            if (check_destruct_pattern_redefinitions_internal(
                    checker, pattern->as.tuple.elements[i], line, col, names, count, capacity)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// Check that a destructuring pattern matches a type (recursive)
// Also sets resolved_type on each pattern node
// Returns 1 if error found, 0 otherwise
static int check_destruct_pattern_against_type(Checker* checker, DestructPattern* pattern,
                                               Type* type, int line, int col) {
    if (!pattern || !type)
        return 1;

    pattern->resolved_type = type;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        // Any type is valid for an identifier pattern
        return 0;

    case PATTERN_TUPLE:
        // Pattern must match a tuple type
        if (type->kind != TYPE_TUPLE && type->kind != TYPE_ERROR) {
            check_error(checker, line, col, "Nested pattern requires a tuple, got '%s'",
                        type_name(type));
            return 1;
        }

        if (type->kind == TYPE_ERROR) {
            // Propagate error type to all children
            for (int i = 0; i < pattern->as.tuple.count; i++) {
                check_destruct_pattern_against_type(checker, pattern->as.tuple.elements[i],
                                                    type_error, line, col);
            }
            return 0;
        }

        // Check arity
        if (type->as.tuple.elem_count != pattern->as.tuple.count) {
            check_error(checker, line, col,
                        "Nested pattern has %d elements, but tuple has %d elements",
                        pattern->as.tuple.count, type->as.tuple.elem_count);
            return 1;
        }

        // Recursively check each element
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            if (check_destruct_pattern_against_type(checker, pattern->as.tuple.elements[i],
                                                    type->as.tuple.elem_types[i], line, col)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// Define variables for all identifiers in a destructuring pattern (recursive)
static void define_destruct_pattern_vars(Checker* checker, DestructPattern* pattern, Type* type,
                                         int is_const, int is_public) {
    if (!pattern)
        return;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        checker_define(checker, pattern->as.ident.name, SYM_VAR, type, is_const, is_public,
                       checker->current_module);
        break;

    case PATTERN_TUPLE:
        if (type->kind == TYPE_TUPLE) {
            for (int i = 0; i < pattern->as.tuple.count; i++) {
                define_destruct_pattern_vars(checker, pattern->as.tuple.elements[i],
                                             type->as.tuple.elem_types[i], is_const, is_public);
            }
        } else {
            // Error type - propagate to all children
            for (int i = 0; i < pattern->as.tuple.count; i++) {
                define_destruct_pattern_vars(checker, pattern->as.tuple.elements[i], type_error,
                                             is_const, is_public);
            }
        }
        break;
    }
}

// --- Statement case helpers ---

static void check_var_decl_stmt(Checker* checker, Node* node) {
    // Check if this is a destructuring declaration
    DestructPattern* pattern = node->as.var_decl.destruct_pattern;
    if (pattern) {
        // Destructuring: var (a, b) = tuple; or var (a, (b, c)) = nested;

        // Check for redefinitions
        if (check_destruct_pattern_redefinitions(checker, pattern, node->line, node->column)) {
            return;
        }

        // Initializer is required (parser enforces this)
        Type* init_type = check_expression(checker, node->as.var_decl.init);

        // Check pattern against initializer type
        if (check_destruct_pattern_against_type(checker, pattern, init_type, node->line,
                                                node->column)) {
            return;
        }

        // Define all variables in the pattern
        define_destruct_pattern_vars(checker, pattern, init_type, node->as.var_decl.is_const,
                                     node->as.var_decl.is_public);
        return;
    }

    // Normal variable declaration
    const char* name = node->as.var_decl.name;

    // Check for redefinition
    if (checker_lookup_local(checker, name)) {
        check_error(checker, node->line, node->column, "Redefinition of '%s'", name);
        return;
    }

    Type* decl_type = NULL;
    Type* init_type = NULL;

    if (node->as.var_decl.type) {
        decl_type = resolve_type(checker, node->as.var_decl.type);
    }

    if (node->as.var_decl.init) {
        // Set enum_target_hint for generic enum inference (e.g., var x: Option<i64> =
        // Option::None)
        Type* old_hint = checker->enum_target_hint;
        if (decl_type && decl_type->kind == TYPE_ENUM) {
            checker->enum_target_hint = decl_type;
        }
        init_type                 = check_expression(checker, node->as.var_decl.init);
        checker->enum_target_hint = old_hint;
    }

    Type* var_type;
    if (decl_type && init_type) {
        // Both specified - check compatibility
        if (!type_assignable(decl_type, init_type)) {
            check_error_type(checker, node->line, node->column, name, decl_type, init_type);
        }
        var_type = decl_type;
    } else if (decl_type) {
        var_type = decl_type;
    } else if (init_type) {
        var_type = init_type;
    } else {
        check_error(checker, node->line, node->column,
                    "Variable '%s' needs type annotation or initializer", name);
        var_type = type_error;
    }

    Symbol* sym = checker_define(checker, name, SYM_VAR, var_type, node->as.var_decl.is_const,
                                 node->as.var_decl.is_public, checker->current_module);

    // Propagate RC tracking
    if (sym && node->as.var_decl.init) {
        if (var_type && var_type->kind == TYPE_ENUM && var_type->as.enm.has_rc_fields) {
            node->as.var_decl.is_rc         = 1;
            node->as.var_decl.resolved_type = var_type;
            sym->is_rc                      = 1;
        }
        if (node->as.var_decl.init->type == NODE_NEW_EXPR) {
            node->as.var_decl.is_rc         = 1;
            node->as.var_decl.resolved_type = node->as.var_decl.init->as.new_expr.resolved_type;
            sym->is_rc                      = 1;
        } else if (node->as.var_decl.init->type == NODE_IDENT) {
            Symbol* src = checker_lookup(checker, node->as.var_decl.init->as.ident.name);
            if (src && src->is_rc) {
                node->as.var_decl.is_rc         = 1;
                node->as.var_decl.resolved_type = src->type;
                sym->is_rc                      = 1;
            }
        } else if (node->as.var_decl.init->type == NODE_CALL && var_type) {
            // Store resolved type for codegen type inference
            node->as.var_decl.resolved_type = var_type;
            if (var_type->kind == TYPE_STRUCT) {
                // Function call returning a struct transfers RC ownership
                node->as.var_decl.is_rc = 1;
                sym->is_rc              = 1;
            }
        }
    }
}

static void check_for_stmt(Checker* checker, Node* node) {
    checker_push_scope(checker); // New scope for init var

    if (node->as.for_stmt.init) {
        check_statement(checker, node->as.for_stmt.init);
    }

    if (node->as.for_stmt.cond) {
        Type* cond = check_expression(checker, node->as.for_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.for_stmt.cond->line, node->as.for_stmt.cond->column,
                        "For condition must be bool, got '%s'", type_name(cond));
        }
    }

    if (node->as.for_stmt.post) {
        check_expression(checker, node->as.for_stmt.post);
    }

    int was_in_loop  = checker->in_loop;
    checker->in_loop = 1;
    check_statement(checker, node->as.for_stmt.body);
    checker->in_loop = was_in_loop;

    checker_pop_scope(checker);
}

static void check_foreach_stmt(Checker* checker, Node* node) {
    checker_push_scope(checker); // New scope for loop variable

    // Check that start and end are integers
    Type* start_type = check_expression(checker, node->as.foreach_stmt.start);
    Type* end_type   = check_expression(checker, node->as.foreach_stmt.end);

    if (!type_is_integer(start_type) && start_type->kind != TYPE_ERROR) {
        check_error(checker, node->as.foreach_stmt.start->line, node->as.foreach_stmt.start->column,
                    "Foreach range start must be int, got '%s'", type_name(start_type));
    }

    if (!type_is_integer(end_type) && end_type->kind != TYPE_ERROR) {
        check_error(checker, node->as.foreach_stmt.end->line, node->as.foreach_stmt.end->column,
                    "Foreach range end must be int, got '%s'", type_name(end_type));
    }

    // Determine loop variable type: prefer end type when start is a default i64 literal
    Type* loop_type;
    if (type_is_integer(end_type) &&
        (start_type->kind == TYPE_INT64 || !type_is_integer(start_type))) {
        loop_type = end_type;
    } else if (type_is_integer(start_type)) {
        loop_type = start_type;
    } else {
        loop_type = type_int64;
    }
    node->as.foreach_stmt.resolved_type = loop_type;

    // Add the loop variable as a const integer (immutable)
    Symbol* sym =
        checker_define(checker, node->as.foreach_stmt.var_name, SYM_VAR, loop_type, 1, 0, NULL);
    if (!sym) {
        check_error(checker, node->line, node->column,
                    "Variable '%s' already declared in this scope", node->as.foreach_stmt.var_name);
    }

    int was_in_loop  = checker->in_loop;
    checker->in_loop = 1;
    check_statement(checker, node->as.foreach_stmt.body);
    checker->in_loop = was_in_loop;

    checker_pop_scope(checker);
}

static void check_return_stmt(Checker* checker, Node* node) {
    Type* expected = checker->current_func_return;
    if (!expected) {
        check_error(checker, node->line, node->column, "Return outside of function");
        return;
    }

    if (node->as.return_stmt.value) {
        // Set enum_target_hint so generic enum constructors can infer from return type
        Type* old_hint = checker->enum_target_hint;
        if (expected->kind == TYPE_ENUM) {
            checker->enum_target_hint = expected;
        }
        Type* actual              = check_expression(checker, node->as.return_stmt.value);
        checker->enum_target_hint = old_hint;
        if (!type_assignable(expected, actual)) {
            check_error_type(checker, node->line, node->column, "Return", expected, actual);
        }
    } else if (expected->kind != TYPE_VOID) {
        check_error(checker, node->line, node->column, "Return without value in non-void function");
    }
}

// =============================================================================
// Statement checking
// =============================================================================

void check_statement(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT:
        check_expression(checker, node->as.expr_stmt.expr);
        break;

    case NODE_VAR_DECL:
        check_var_decl_stmt(checker, node);
        break;

    case NODE_BLOCK:
        checker_push_scope(checker);
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            check_statement(checker, node->as.block.stmts.nodes[i]);
        }
        checker_pop_scope(checker);
        break;

    case NODE_IF: {
        Type* cond = check_expression(checker, node->as.if_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.if_stmt.cond->line, node->as.if_stmt.cond->column,
                        "If condition must be bool, got '%s'", type_name(cond));
        }
        check_statement(checker, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_block) {
            check_statement(checker, node->as.if_stmt.else_block);
        }
        break;
    }

    case NODE_WHILE: {
        Type* cond = check_expression(checker, node->as.while_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.while_stmt.cond->line, node->as.while_stmt.cond->column,
                        "While condition must be bool, got '%s'", type_name(cond));
        }
        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.while_stmt.body);
        checker->in_loop = was_in_loop;
        break;
    }

    case NODE_FOR:
        check_for_stmt(checker, node);
        break;

    case NODE_FOREACH:
        check_foreach_stmt(checker, node);
        break;

    case NODE_RETURN:
        check_return_stmt(checker, node);
        break;

    case NODE_BREAK:
        if (!checker->in_loop) {
            check_error(checker, node->line, node->column, "Break outside of loop");
        }
        break;

    case NODE_CONTINUE:
        if (!checker->in_loop) {
            check_error(checker, node->line, node->column, "Continue outside of loop");
        }
        break;

    case NODE_DEFER:
        if (!checker->current_func_return) {
            check_error(checker, node->line, node->column, "Defer outside of function");
            return;
        }
        check_statement(checker, node->as.defer_stmt.stmt);
        break;

    default:
        check_error(checker, node->line, node->column, "Unknown statement type %d", node->type);
        break;
    }
}

// =============================================================================
// Declaration checking
// =============================================================================

static Type* get_function_type(Checker* checker, Node* node) {
    func_decl_node* fdn = &(node->as.func_decl);

    int    param_count = fdn->params.count;
    Type** param_types = NULL;
    if (param_count > 0) {
        param_types = xmalloc(param_count * sizeof(Type*));
    }

    Type* return_type = type_void;
    if (fdn->return_type) {
        return_type = resolve_type(checker, fdn->return_type);
    }

    // Build function type
    for (int i = 0; i < param_count; i++) {
        Node* param = fdn->params.nodes[i];
        Type* ptype = type_void;
        if (param->as.param.type) {
            ptype = resolve_type(checker, param->as.param.type);
        }
        param_types[i] = ptype;
    }

    Type* func_type = type_func(param_types, param_count, return_type, fdn->is_varargs);

    return func_type;
}

static void check_extern_module_decl(Checker* checker, Node* node) {
    for (int i = 0; i < node->as.extern_module.decls.count; i++) {
        Node* decl = node->as.extern_module.decls.nodes[i];
        if (decl->type == NODE_FUNC_DECL) {
            func_decl_node* fdn       = &decl->as.func_decl;
            Type*           func_type = get_function_type(checker, decl);

            checker_define(checker, fdn->name, SYM_FUNC, func_type, 0, fdn->is_public,
                           checker->current_module);
        }
    }
}

static void check_func_decl(Checker* checker, Node* node) {
    func_decl_node* fdn = &node->as.func_decl;

    const char* name          = fdn->name;
    const char* receiver_type = fdn->receiver_type;
    int         is_method     = (receiver_type != NULL);

    // Check if this is a method on a generic struct: func (Box<T>) get(): T
    // or func (Pair<i32, Box<T>>) set(): void
    if (is_method && fdn->receiver_type_args.count > 0) {
        // Look up the generic definition
        GenericDef* def = lookup_generic_def(checker, receiver_type);
        if (!def) {
            check_error(checker, node->line, node->column, "Unknown generic type '%s'",
                        receiver_type);
            return;
        }
        // Verify type arg arity matches
        if (fdn->receiver_type_args.count != def->type_param_count) {
            check_error(checker, node->line, node->column,
                        "Generic type '%s' expects %d type parameters, got %d", receiver_type,
                        def->type_param_count, fdn->receiver_type_args.count);
            return;
        }
        // Store the method on the generic definition - will be instantiated later
        register_generic_method(def, node);
        return;
    }

    // For methods, use mangled name: StructName_methodName
    char* mangled_name = NULL;
    if (is_method) {
        size_t len   = strlen(receiver_type) + 1 + strlen(name) + 1;
        mangled_name = xmalloc(len);
        snprintf(mangled_name, len, "%s_%s", receiver_type, name);
    } else {
        mangled_name = xstrdup(name);
    }

    // if main function, ensure correct signature
    if (!is_method && strcmp(name, "main") == 0) {
        if (fdn->params.count != 0) {
            check_error(checker, node->line, node->column,
                        "main function must not have parameters");
        }
        if (fdn->return_type) {
            Type* ret_type = resolve_type(checker, fdn->return_type);
            if (ret_type->kind != TYPE_INT32) {
                check_error(checker, node->line, node->column,
                            "main function must have return type i32");
            }
        } else {
            check_error(checker, node->line, node->column,
                        "main function must have return type i32");
        }
    }

    // Check for redefinition
    if (checker_lookup(checker, mangled_name)) {
        check_error(checker, node->line, node->column, "Redefinition of '%s'", mangled_name);
        free(mangled_name);
        return;
    }

    Type* func_type = get_function_type(checker, node);

    // Pre-declare function for recursion
    checker_define(checker, mangled_name, SYM_FUNC, func_type, 1, fdn->is_public,
                   checker->current_module);

    // For methods, also register the method on the struct type
    if (is_method) {
        Symbol* struct_sym = checker_lookup(checker, receiver_type);
        if (!struct_sym || struct_sym->kind != SYM_TYPE || struct_sym->type->kind != TYPE_STRUCT) {
            check_error(checker, node->line, node->column, "Unknown receiver type '%s'",
                        receiver_type);
        } else {
            Type* st = struct_sym->type;
            int   n  = st->as.struc.method_count;

            st->as.struc.method_names =
                xrealloc(st->as.struc.method_names, (n + 1) * sizeof(char*));
            st->as.struc.method_types =
                xrealloc(st->as.struc.method_types, (n + 1) * sizeof(Type*));
            st->as.struc.method_is_const =
                xrealloc(st->as.struc.method_is_const, (n + 1) * sizeof(int));

            st->as.struc.method_names[n]    = xstrdup(name);
            st->as.struc.method_types[n]    = func_type;
            st->as.struc.method_is_const[n] = fdn->receiver_is_const;
            st->as.struc.method_count       = n + 1;
        }
    }

    // Enter function scope
    checker_push_scope(checker);
    Type* old_return             = checker->current_func_return;
    checker->current_func_return = func_type->as.func.return_type;

    // Set this function's accessible modules for visibility checking
    char** old_accessible_modules             = checker->current_accessible_modules;
    int    old_accessible_modules_count       = checker->current_accessible_modules_count;
    checker->current_accessible_modules       = fdn->accessible_modules;
    checker->current_accessible_modules_count = fdn->accessible_modules_count;

    // For methods, inject 'self' into scope
    // self is a struct reference (the struct type itself, with reference semantics)
    if (is_method) {
        Symbol* struct_sym = checker_lookup(checker, receiver_type);
        if (struct_sym && struct_sym->kind == SYM_TYPE) {
            Type* self_type = struct_sym->type;
            checker_define(checker, "self", SYM_VAR, self_type, fdn->receiver_is_const, 0, NULL);
        }
    }

    // Define parameters
    for (int i = 0; i < func_type->as.func.param_count; i++) {
        Node* param = fdn->params.nodes[i];
        Type* ptype = func_type->as.func.param_types[i];

        if (!checker_define(checker, param->as.param.name, SYM_VAR, ptype, param->as.param.is_const,
                            0, NULL)) {
            check_error(checker, param->line, param->column, "Duplicate parameter name '%s'",
                        param->as.param.name);
        }
    }

    // Check body
    if (fdn->body) {
        // Body is a block, but we already pushed scope for params
        // So just check the statements directly
        Node* body = fdn->body;
        for (int i = 0; i < body->as.block.stmts.count; i++) {
            check_statement(checker, body->as.block.stmts.nodes[i]);
        }
    }

    // Restore previous accessible modules context
    checker->current_accessible_modules       = old_accessible_modules;
    checker->current_accessible_modules_count = old_accessible_modules_count;

    checker->current_func_return = old_return;
    checker_pop_scope(checker);
    free(mangled_name);
}

static void check_struct_decl(Checker* checker, Node* node) {
    const char* name = node->as.struct_decl.name;

    // Check for redefinition
    if (checker_lookup(checker, name)) {
        check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
        return;
    }

    // Check if this is a generic struct definition
    if (node->as.struct_decl.type_param_count > 0) {
        // Register as generic template - don't create concrete type yet
        register_generic_def(checker, name, node->as.struct_decl.type_params,
                             node->as.struct_decl.type_param_bounds,
                             node->as.struct_decl.type_param_count, node);
        return;
    }

    // Non-generic struct: create concrete type as before
    Type* struct_type = type_struct(name);
    int   field_count = node->as.struct_decl.fields.count;

    struct_type->as.struc.field_count = field_count;
    struct_type->as.struc.field_names = xmalloc(field_count * sizeof(char*));
    struct_type->as.struc.field_types = xmalloc(field_count * sizeof(Type*));

    for (int i = 0; i < field_count; i++) {
        Node* field                          = node->as.struct_decl.fields.nodes[i];
        struct_type->as.struc.field_names[i] = xstrdup(field->as.field.name);
        struct_type->as.struc.field_types[i] = resolve_type(checker, field->as.field.type);
    }

    // Check if any field is an RC-managed type (struct, Vec, or enum with RC fields)
    for (int i = 0; i < field_count; i++) {
        Type* ftype = struct_type->as.struc.field_types[i];
        if (ftype && (ftype->kind == TYPE_STRUCT || ftype->kind == TYPE_VEC ||
                      (ftype->kind == TYPE_ENUM && ftype->as.enm.has_rc_fields))) {
            struct_type->as.struc.has_rc_fields = 1;
            break;
        }
    }

    checker_define(checker, name, SYM_TYPE, struct_type, 0, node->as.struct_decl.is_public,
                   checker->current_module);
}

static void check_enum_decl(Checker* checker, Node* node) {
    const char* name = node->as.enum_decl.name;

    if (checker_lookup(checker, name)) {
        check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
        return;
    }

    // Check if this is a generic enum definition
    if (node->as.enum_decl.type_param_count > 0) {
        register_generic_def(checker, name, node->as.enum_decl.type_params,
                             node->as.enum_decl.type_param_bounds,
                             node->as.enum_decl.type_param_count, node);
        return;
    }

    Type* enum_type   = type_enum(name);
    int   value_count = node->as.enum_decl.values.count;

    enum_type->as.enm.value_count         = value_count;
    enum_type->as.enm.value_names         = xmalloc(value_count * sizeof(char*));
    enum_type->as.enm.variant_types       = xmalloc(value_count * sizeof(Type**));
    enum_type->as.enm.variant_type_counts = xmalloc(value_count * sizeof(int));

    checker_define(checker, name, SYM_TYPE, enum_type, 0, node->as.enum_decl.is_public,
                   checker->current_module);

    // Process enum variants
    for (int i = 0; i < value_count; i++) {
        Node* val                        = node->as.enum_decl.values.nodes[i];
        enum_type->as.enm.value_names[i] = xstrdup(val->as.enum_variant.name);

        int type_count                           = val->as.enum_variant.types.count;
        enum_type->as.enm.variant_type_counts[i] = type_count;

        if (type_count > 0) {
            enum_type->as.enm.has_data         = 1;
            enum_type->as.enm.variant_types[i] = xmalloc(type_count * sizeof(Type*));
            for (int j = 0; j < type_count; j++) {
                Type* resolved = resolve_type(checker, val->as.enum_variant.types.nodes[j]);
                enum_type->as.enm.variant_types[i][j] = resolved;
                if (resolved && (resolved->kind == TYPE_STRUCT ||
                                 (resolved->kind == TYPE_ENUM && resolved->as.enm.has_rc_fields))) {
                    enum_type->as.enm.has_rc_fields = 1;
                }
            }
        } else {
            enum_type->as.enm.variant_types[i] = NULL;
        }
    }
}

static void check_trait_decl(Checker* checker, Node* node) {
    const char* name = node->as.trait_decl.name;

    // Check for redefinition
    if (checker_lookup(checker, name)) {
        check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
        return;
    }

    // Create TYPE_TRAIT with method signatures
    Type* trait_type   = type_trait(name);
    int   method_count = node->as.trait_decl.methods.count;

    trait_type->as.trait.method_count    = method_count;
    trait_type->as.trait.method_names    = xmalloc(method_count * sizeof(char*));
    trait_type->as.trait.method_types    = xmalloc(method_count * sizeof(Type*));
    trait_type->as.trait.method_is_const = xmalloc(method_count * sizeof(int));

    for (int i = 0; i < method_count; i++) {
        Node* method                            = node->as.trait_decl.methods.nodes[i];
        trait_type->as.trait.method_names[i]    = xstrdup(method->as.func_decl.name);
        trait_type->as.trait.method_types[i]    = get_function_type(checker, method);
        trait_type->as.trait.method_is_const[i] = method->as.func_decl.receiver_is_const;
    }

    checker_define(checker, name, SYM_TYPE, trait_type, 0, node->as.trait_decl.is_public,
                   checker->current_module);
}

static void check_type_alias_decl(Checker* checker, Node* node) {
    const char* name = node->as.type_alias.name;

    // Check for redefinition
    if (checker_lookup(checker, name)) {
        check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
        return;
    }

    // Generic type alias: register as a generic def template
    if (node->as.type_alias.type_param_count > 0) {
        register_generic_def(checker, name, node->as.type_alias.type_params,
                             node->as.type_alias.type_param_bounds,
                             node->as.type_alias.type_param_count, node);
        // Mark as type alias so resolve_type can handle it differently
        GenericDef* def    = lookup_generic_def(checker, name);
        def->is_type_alias = 1;
        return;
    }

    // Non-generic type alias: resolve the target type and define the symbol
    checker->alias_depth++;
    if (checker->alias_depth > 16) {
        check_error(checker, node->line, node->column, "Recursive type alias '%s'", name);
        checker->alias_depth--;
        return;
    }
    Type* resolved = resolve_type(checker, node->as.type_alias.target_type);
    checker->alias_depth--;

    if (resolved == type_error) {
        return;
    }

    checker_define(checker, name, SYM_TYPE, resolved, 0, node->as.type_alias.is_public,
                   checker->current_module);
}

static void check_impl_decl(Checker* checker, Node* node) {
    const char* trait_name    = node->as.impl_decl.trait_name;
    const char* type_name_str = node->as.impl_decl.type_name;

    // Look up the trait
    Symbol* trait_sym = checker_lookup(checker, trait_name);
    if (!trait_sym || trait_sym->kind != SYM_TYPE || trait_sym->type->kind != TYPE_TRAIT) {
        check_error(checker, node->line, node->column, "Unknown trait '%s'", trait_name);
        return;
    }
    Type* trait_type = trait_sym->type;

    // Look up the target type
    Symbol*     type_sym    = checker_lookup(checker, type_name_str);
    GenericDef* generic_def = NULL;
    int         is_generic  = 0;
    if (!type_sym || type_sym->kind != SYM_TYPE || type_sym->type->kind != TYPE_STRUCT) {
        // Fallback: check if it's a generic struct template
        generic_def = lookup_generic_def(checker, type_name_str);
        if (!generic_def) {
            check_error(checker, node->line, node->column,
                        "Cannot implement trait for unknown struct type '%s'", type_name_str);
            return;
        }
        is_generic = 1;
    }

    // Process each method in the impl block
    for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
        Node* method = node->as.impl_decl.methods.nodes[i];

        // Verify this method exists in the trait
        const char* method_name      = method->as.func_decl.name;
        int         found_in_trait   = 0;
        int         trait_method_idx = -1;
        for (int j = 0; j < trait_type->as.trait.method_count; j++) {
            if (strcmp(trait_type->as.trait.method_names[j], method_name) == 0) {
                found_in_trait   = 1;
                trait_method_idx = j;
                break;
            }
        }

        if (!found_in_trait) {
            check_error(checker, method->line, method->column,
                        "Method '%s' is not declared in trait '%s'", method_name, trait_name);
            continue;
        }

        // Check receiver const-ness matches trait declaration
        int trait_is_const = trait_type->as.trait.method_is_const[trait_method_idx];
        int impl_is_const  = method->as.func_decl.receiver_is_const;
        if (impl_is_const != trait_is_const) {
            check_error(checker, method->line, method->column,
                        "Method '%s' receiver mutability mismatch: trait '%s' declares '%s', "
                        "impl provides '%s'",
                        method_name, trait_name, trait_is_const ? "const func" : "func",
                        impl_is_const ? "const func" : "func");
            continue;
        }

        if (is_generic) {
            // For generic structs, the method has a generic receiver (e.g., func (Box<T>)
            // drop()) Process it via check_decl which routes to the generic method registration
            // path
            check_decl(checker, method);
        } else {
            // Verify the method signature matches the trait signature
            Type* impl_func_type  = get_function_type(checker, method);
            Type* trait_func_type = trait_type->as.trait.method_types[trait_method_idx];

            // Check return type
            if (!type_equals(impl_func_type->as.func.return_type,
                             trait_func_type->as.func.return_type)) {
                check_error(checker, method->line, method->column,
                            "Method '%s' return type mismatch: trait '%s' expects '%s', got '%s'",
                            method_name, trait_name,
                            type_name(trait_func_type->as.func.return_type),
                            type_name(impl_func_type->as.func.return_type));
                continue;
            }

            // Check parameter types
            if (impl_func_type->as.func.param_count != trait_func_type->as.func.param_count) {
                check_error(checker, method->line, method->column,
                            "Method '%s' parameter count mismatch: trait '%s' expects %d, got %d",
                            method_name, trait_name, trait_func_type->as.func.param_count,
                            impl_func_type->as.func.param_count);
                continue;
            }

            for (int p = 0; p < trait_func_type->as.func.param_count; p++) {
                if (!type_equals(impl_func_type->as.func.param_types[p],
                                 trait_func_type->as.func.param_types[p])) {
                    check_error(
                        checker, method->line, method->column,
                        "Method '%s' parameter %d type mismatch: trait '%s' expects '%s', got "
                        "'%s'",
                        method_name, p + 1, trait_name,
                        type_name(trait_func_type->as.func.param_types[p]),
                        type_name(impl_func_type->as.func.param_types[p]));
                }
            }

            // Process the method as a regular func_decl (registers on struct, checks body)
            check_decl(checker, method);
        }
    }

    // Verify all required trait methods are implemented
    for (int j = 0; j < trait_type->as.trait.method_count; j++) {
        const char* required    = trait_type->as.trait.method_names[j];
        int         implemented = 0;
        for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
            if (strcmp(node->as.impl_decl.methods.nodes[i]->as.func_decl.name, required) == 0) {
                implemented = 1;
                break;
            }
        }
        if (!implemented) {
            check_error(checker, node->line, node->column,
                        "Missing required method '%s' from trait '%s'", required, trait_name);
        }
    }

    // Record the trait implementation
    VEC_GROW(checker->trait_impls, checker->trait_impl_count, checker->trait_impl_capacity);
    TraitImpl* impl  = &checker->trait_impls[checker->trait_impl_count++];
    impl->trait_name = xstrdup(trait_name);
    impl->type_name  = xstrdup(type_name_str);

    // If implementing Drop, set the flag on the struct type
    if (strcmp(trait_name, "Drop") == 0 && !is_generic) {
        type_sym->type->as.struc.has_drop = 1;
    }
}

static void check_decl(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXTERN_MODULE:
        check_extern_module_decl(checker, node);
        break;

    case NODE_FUNC_DECL:
        check_func_decl(checker, node);
        break;

    case NODE_STRUCT_DECL:
        check_struct_decl(checker, node);
        break;

    case NODE_ENUM_DECL:
        check_enum_decl(checker, node);
        break;

    case NODE_TRAIT_DECL:
        check_trait_decl(checker, node);
        break;

    case NODE_TYPE_ALIAS:
        check_type_alias_decl(checker, node);
        break;

    case NODE_IMPL_DECL:
        check_impl_decl(checker, node);
        break;

    case NODE_VAR_DECL:
        // Global variable
        check_statement(checker, node);
        break;

    default:
        check_error(checker, node->line, node->column, "Unknown declaration type %d", node->type);
        break;
    }
}

// =============================================================================
// Main entry point
// =============================================================================

int checker_check(Checker* checker, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM) {
        return 0;
    }

    checker_push_scope(checker); // Global scope

    // First pass: declare all types (structs, enums, traits) for forward references
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        checker->current_module =
            strcmp(mod->as.module.name, "main") == 0 ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL ||
                decl->type == NODE_TRAIT_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    // Second pass: register generic methods and trait impls on GenericDefs.
    // This must happen before type aliases, which may trigger generic instantiation.
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        checker->current_module =
            strcmp(mod->as.module.name, "main") == 0 ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            // Generic method: func (Box<T>) get(): T
            if (decl->type == NODE_FUNC_DECL && decl->as.func_decl.receiver_type &&
                decl->as.func_decl.receiver_type_args.count > 0) {
                check_decl(checker, decl);
            }
            // Generic impl block: impl Drop for Box<T>
            else if (decl->type == NODE_IMPL_DECL && decl->as.impl_decl.type_args.count > 0) {
                check_decl(checker, decl);
            }
        }
    }

    // Third pass: check everything else in library modules (including type aliases)
    // Process library modules (non-main) first, then main module last
    // This ensures library functions are declared before main uses them
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        if (strcmp(mod->as.module.name, "main") == 0)
            continue;
        checker->current_module = mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            // Skip already-processed declarations
            if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL ||
                decl->type == NODE_TRAIT_DECL) {
                continue;
            }
            if (decl->type == NODE_FUNC_DECL && decl->as.func_decl.receiver_type &&
                decl->as.func_decl.receiver_type_args.count > 0) {
                continue;
            }
            if (decl->type == NODE_IMPL_DECL && decl->as.impl_decl.type_args.count > 0) {
                continue;
            }
            check_decl(checker, decl);
        }
    }

    // Fourth pass: check main module (including type aliases)
    // Type aliases that trigger generic instantiation now have access to all library symbols
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        if (strcmp(mod->as.module.name, "main") != 0)
            continue;
        checker->current_module = NULL;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            // Skip already-processed declarations
            if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL ||
                decl->type == NODE_TRAIT_DECL) {
                continue;
            }
            if (decl->type == NODE_FUNC_DECL && decl->as.func_decl.receiver_type &&
                decl->as.func_decl.receiver_type_args.count > 0) {
                continue;
            }
            if (decl->type == NODE_IMPL_DECL && decl->as.impl_decl.type_args.count > 0) {
                continue;
            }
            check_decl(checker, decl);
        }
    }

    checker->current_module = NULL;
    checker_pop_scope(checker);

    return checker->error_count == 0;
}
