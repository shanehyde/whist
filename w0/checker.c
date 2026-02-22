#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "checker_internal.h"
#include "sem_info.h"
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

// Forward declaration for match checking
static void  check_match_stmt(Checker* checker, Node* node);
static Type* check_match(Checker* checker, Node* node, int is_expr_context);

// Forward declarations for check_decl helpers
static void check_extern_module_decl(Checker* checker, Node* node);
static void check_func_decl(Checker* checker, Node* node);
static void check_struct_decl(Checker* checker, Node* node);
static void check_enum_decl(Checker* checker, Node* node);
static void check_trait_decl(Checker* checker, Node* node);
static void check_type_alias_decl(Checker* checker, Node* node);
static void check_impl_decl(Checker* checker, Node* node);
static void check_use_decl(Checker* checker, Node* node);
static void check_test_decl(Checker* checker, Node* node);
static int  is_prelude_symbol(Symbol* sym);

// =============================================================================
// Utility functions
// =============================================================================

// Report a type-checking error at the given source location
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

// Compute a djb2 hash for symbol table indexing
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

// Initialize all checker state to zero/NULL defaults
void checker_init(Checker* checker) {
    checker->scope                                    = NULL;
    checker->current_func_return                      = NULL;
    checker->in_loop                                  = 0;
    checker->error_count                              = 0;
    checker->modules.direct_imports                   = NULL;
    checker->modules.direct_imports_count             = 0;
    checker->modules.current_accessible_modules       = NULL;
    checker->modules.current_accessible_modules_count = 0;
    checker->modules.current_module                   = NULL;
    // Generic support
    checker->generics.defs                     = NULL;
    checker->generics.def_count                = 0;
    checker->generics.def_capacity             = 0;
    checker->generics.instances                = NULL;
    checker->generics.instance_count           = 0;
    checker->generics.instance_capacity        = 0;
    checker->generics.current_type_params      = NULL;
    checker->generics.current_type_args        = NULL;
    checker->generics.current_type_param_count = 0;
    // Generic free functions
    checker->generics.func_defs              = NULL;
    checker->generics.func_def_count         = 0;
    checker->generics.func_def_capacity      = 0;
    checker->generics.func_instances         = NULL;
    checker->generics.func_instance_count    = 0;
    checker->generics.func_instance_capacity = 0;
    // Span support
    checker->containers.spans         = NULL;
    checker->containers.span_count    = 0;
    checker->containers.span_capacity = 0;
    // Vec support
    checker->containers.vecs         = NULL;
    checker->containers.vec_count    = 0;
    checker->containers.vec_capacity = 0;
    // Box support
    checker->containers.boxes        = NULL;
    checker->containers.box_count    = 0;
    checker->containers.box_capacity = 0;
    // Trait support
    checker->traits.impls         = NULL;
    checker->traits.impl_count    = 0;
    checker->traits.impl_capacity = 0;
    // Primitive methods
    checker->traits.primitive_methods         = NULL;
    checker->traits.primitive_method_count    = 0;
    checker->traits.primitive_method_capacity = 0;
    // Deferred trait checks
    checker->traits.deferred_checks         = NULL;
    checker->traits.deferred_check_count    = 0;
    checker->traits.deferred_check_capacity = 0;
    checker->alias_depth                    = 0;
    checker->enum_target_hint               = NULL;
    checker->expected_func_type             = NULL;
    checker->self_type                      = NULL;
    checker->current_method_receiver        = NULL;
    checker->lambda_next_id                 = 0;
    checker->lambda_depth                   = 0;
    checker->lambda_stack                   = NULL;
    checker->lambda_stack_count             = 0;
    checker->lambda_stack_capacity          = 0;
    checker->sem                            = sem_info_new();
    types_init();
}

// Set the list of directly imported module names for visibility checking
void checker_set_direct_imports(Checker* checker, char** direct_imports, int count) {
    checker->modules.direct_imports       = direct_imports;
    checker->modules.direct_imports_count = count;
}

// Push a new scope onto the scope chain for block-level symbol resolution
void checker_push_scope(Checker* checker) {
    Scope* scope              = xcalloc(1, sizeof(Scope));
    scope->symbols            = xcalloc(SCOPE_SIZE, sizeof(Symbol*));
    scope->size               = SCOPE_SIZE;
    scope->parent             = checker->scope;
    scope->is_lambda_boundary = 0;
    checker->scope            = scope;
}

// Pop the current scope and free all its symbols
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

// Free all checker resources: scopes, generic defs/instances, spans, vecs, trait impls
void checker_free(Checker* checker) {
    while (checker->scope) {
        checker_pop_scope(checker);
    }
    // Free generic definitions
    for (int i = 0; i < checker->generics.def_count; i++) {
        free(checker->generics.defs[i].name);
        for (int j = 0; j < checker->generics.defs[i].type_param_count; j++) {
            free(checker->generics.defs[i].type_params[j]);
            free(checker->generics.defs[i].type_param_bounds[j]);
        }
        free(checker->generics.defs[i].type_params);
        free(checker->generics.defs[i].type_param_bounds);
        free((void*)checker->generics.defs[i].source_module);
        // Note: methods array contains pointers to AST nodes, don't free them
        free(checker->generics.defs[i].methods);
    }
    free(checker->generics.defs);
    // Free generic instances
    for (int i = 0; i < checker->generics.instance_count; i++) {
        free(checker->generics.instances[i].mangled_name);
        free(checker->generics.instances[i].base_name);
        free(checker->generics.instances[i].type_args);
    }
    free(checker->generics.instances);
    // Free generic free function definitions
    for (int i = 0; i < checker->generics.func_def_count; i++) {
        free(checker->generics.func_defs[i].name);
        for (int j = 0; j < checker->generics.func_defs[i].type_param_count; j++) {
            free(checker->generics.func_defs[i].type_params[j]);
            free(checker->generics.func_defs[i].type_param_bounds[j]);
        }
        free(checker->generics.func_defs[i].type_params);
        free(checker->generics.func_defs[i].type_param_bounds);
        free((void*)checker->generics.func_defs[i].source_module);
        free(checker->generics.func_defs[i].receiver_type);
    }
    free(checker->generics.func_defs);
    // Free generic free function instances
    for (int i = 0; i < checker->generics.func_instance_count; i++) {
        free(checker->generics.func_instances[i].mangled_name);
        free(checker->generics.func_instances[i].base_name);
        free(checker->generics.func_instances[i].type_args);
        free(checker->generics.func_instances[i].receiver_type);
        node_free(checker->generics.func_instances[i].body);
    }
    free(checker->generics.func_instances);
    // Free span instances
    for (int i = 0; i < checker->containers.span_count; i++) {
        free(checker->containers.spans[i].mangled_name);
    }
    free(checker->containers.spans);
    // Free vec instances
    for (int i = 0; i < checker->containers.vec_count; i++) {
        free(checker->containers.vecs[i].mangled_name);
        for (int j = 0; j < checker->containers.vecs[i].method_count; j++) {
            free(checker->containers.vecs[i].method_names[j]);
        }
        free(checker->containers.vecs[i].method_names);
        free(checker->containers.vecs[i].method_types);
        free(checker->containers.vecs[i].method_is_const);
        // method_bodies are AST nodes — freed by node_free on cloned bodies
        free(checker->containers.vecs[i].method_bodies);
    }
    free(checker->containers.vecs);
    // Free box instances
    for (int i = 0; i < checker->containers.box_count; i++) {
        free(checker->containers.boxes[i].mangled_name);
    }
    free(checker->containers.boxes);
    // Free trait implementations
    for (int i = 0; i < checker->traits.impl_count; i++) {
        free(checker->traits.impls[i].trait_name);
        free(checker->traits.impls[i].type_name);
    }
    free(checker->traits.impls);
    // Free primitive methods
    for (int i = 0; i < checker->traits.primitive_method_count; i++) {
        free(checker->traits.primitive_methods[i].type_name);
        free(checker->traits.primitive_methods[i].method_name);
    }
    free(checker->traits.primitive_methods);
    // Free deferred trait checks
    for (int i = 0; i < checker->traits.deferred_check_count; i++) {
        free(checker->traits.deferred_checks[i].type_name);
        free(checker->traits.deferred_checks[i].method_name);
    }
    free(checker->traits.deferred_checks);
    free(checker->lambda_stack);
    sem_info_free(checker->sem);
    checker->sem = NULL;
    types_cleanup();
}

// =============================================================================
// Symbol table operations
// =============================================================================

// Define a symbol in the current scope. Returns NULL if already defined.
Symbol* checker_define(Checker* checker, const char* name, SymbolKind kind, Type* type,
                       int is_const, int is_public, const char* source_module) {
    Scope*       scope = checker->scope;
    unsigned int index = hash_string(name) % scope->size;

    // Check for redefinition in current scope
    for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            // Allow shadowing prelude symbols
            if (sym->source_module && strcmp(sym->source_module, "prelude") == 0) {
                continue;
            }
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

    // Prelude symbols are always accessible from any context
    if (strcmp(source_module, "prelude") == 0) {
        return 1;
    }

    // If we're currently in the same module, it's accessible
    if (checker->modules.current_module &&
        strcmp(checker->modules.current_module, source_module) == 0) {
        return 1;
    }

    // Use current function's accessible modules if set, otherwise use global direct_imports
    char** modules = checker->modules.current_accessible_modules;
    int    count   = checker->modules.current_accessible_modules_count;

    if (!modules) {
        modules = checker->modules.direct_imports;
        count   = checker->modules.direct_imports_count;
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
    // Prelude is always considered imported
    if (strcmp(name, "prelude") == 0) {
        return 1;
    }

    // Use current function's accessible modules if set, otherwise use global direct_imports
    char** modules = checker->modules.current_accessible_modules;
    int    count   = checker->modules.current_accessible_modules_count;

    if (!modules) {
        modules = checker->modules.direct_imports;
        count   = checker->modules.direct_imports_count;
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

// Look up a symbol by name, respecting module visibility and qualification rules
Symbol* checker_lookup(Checker* checker, const char* name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                // Check module visibility
                if (!is_module_accessible(checker, sym->source_module)) {
                    continue; // Symbol exists but not accessible from this module
                }
                // Prelude symbols are accessible without module qualification
                int is_prelude = sym->source_module && strcmp(sym->source_module, "prelude") == 0;
                // Symbols from library imports require module qualification
                // Skip them here - they must be accessed via module.symbol syntax
                int same_module =
                    (sym->source_module == NULL && checker->modules.current_module == NULL) ||
                    (sym->source_module && checker->modules.current_module &&
                     strcmp(sym->source_module, checker->modules.current_module) == 0);
                if (sym->source_module && !same_module && !is_prelude) {
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

// Look up a symbol in the current scope only (no parent traversal)
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

// Recursive helper: collect pattern identifiers and check for duplicates or scope conflicts
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

    case PATTERN_STRUCT:
        for (int i = 0; i < pattern->as.struc.count; i++) {
            const char* name = pattern->as.struc.local_names[i];

            if (checker_lookup_local(checker, name)) {
                check_error(checker, line, col, "Redefinition of '%s'", name);
                return 1;
            }

            for (int j = 0; j < *count; j++) {
                if (strcmp((*names)[j], name) == 0) {
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
        }
        return 0;
    }
    return 0;
}

// Propagate tuple-pattern child types as type_error when parent type checking already failed.
static void propagate_tuple_pattern_error(Checker* checker, DestructPattern* pattern, int line,
                                          int col) {
    for (int i = 0; i < pattern->as.tuple.count; i++) {
        check_destruct_pattern_against_type(checker, pattern->as.tuple.elements[i], type_error,
                                            line, col);
    }
}

// Validate tuple destructuring arity and recursively validate each element type.
static int check_tuple_destruct_pattern_against_type(Checker* checker, DestructPattern* pattern,
                                                     Type* type, int line, int col) {
    if (type->kind != TYPE_TUPLE && type->kind != TYPE_ERROR) {
        check_error(checker, line, col, "Nested pattern requires a tuple, got '%s'",
                    type_name(type));
        return 1;
    }

    if (type->kind == TYPE_ERROR) {
        propagate_tuple_pattern_error(checker, pattern, line, col);
        return 0;
    }

    if (type->as.tuple.elem_count != pattern->as.tuple.count) {
        check_error(checker, line, col, "Nested pattern has %d elements, but tuple has %d elements",
                    pattern->as.tuple.count, type->as.tuple.elem_count);
        return 1;
    }

    for (int i = 0; i < pattern->as.tuple.count; i++) {
        if (check_destruct_pattern_against_type(checker, pattern->as.tuple.elements[i],
                                                type->as.tuple.elem_types[i], line, col)) {
            return 1;
        }
    }
    return 0;
}

// Validate struct destructuring fields and record resolved field types.
static int check_struct_destruct_pattern_against_type(Checker* checker, DestructPattern* pattern,
                                                      Type* type, int line, int col) {
    if (type->kind != TYPE_STRUCT && type->kind != TYPE_ERROR) {
        check_error(checker, line, col, "Struct destructuring requires a struct type, got '%s'",
                    type_name(type));
        return 1;
    }

    if (type->kind == TYPE_ERROR) {
        for (int i = 0; i < pattern->as.struc.count; i++) {
            pattern->as.struc.field_types[i] = type_error;
        }
        return 0;
    }

    for (int i = 0; i < pattern->as.struc.count; i++) {
        int field_idx = type_find_field_index(type, pattern->as.struc.field_names[i]);
        if (field_idx < 0) {
            check_error(checker, line, col, "Struct '%s' has no field '%s'", type->as.struc.name,
                        pattern->as.struc.field_names[i]);
            return 1;
        }
        pattern->as.struc.field_types[i] = type->as.struc.field_types[field_idx];
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
        return 0;
    case PATTERN_TUPLE:
        return check_tuple_destruct_pattern_against_type(checker, pattern, type, line, col);
    case PATTERN_STRUCT:
        return check_struct_destruct_pattern_against_type(checker, pattern, type, line, col);
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
                       checker->modules.current_module);
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

    case PATTERN_STRUCT:
        for (int i = 0; i < pattern->as.struc.count; i++) {
            checker_define(checker, pattern->as.struc.local_names[i], SYM_VAR,
                           pattern->as.struc.field_types[i], is_const, is_public,
                           checker->modules.current_module);
        }
        break;
    }
}

// --- Statement case helpers ---

static int is_module_call_enum_value(Node* init) {
    return init->type == NODE_ENUM_VALUE && init->as.enum_value.is_module_call;
}

static void mark_var_decl_rc(Node* node, Symbol* sym, Type* resolved_type) {
    node->as.var_decl.is_rc         = 1;
    node->as.var_decl.resolved_type = resolved_type;
    if (sym) {
        sym->is_rc = 1;
    }
}

static Type* check_var_decl_initializer(Checker* checker, Type* decl_type, Node* init) {
    if (!init) {
        return NULL;
    }

    // Set enum_target_hint for generic enum inference (e.g., var x: Option<i64> = Option::None)
    Type* old_hint = checker->enum_target_hint;
    if (decl_type && decl_type->kind == TYPE_ENUM) {
        checker->enum_target_hint = decl_type;
    }
    Type* init_type           = check_expression(checker, init);
    checker->enum_target_hint = old_hint;
    return init_type;
}

static Type* resolve_var_decl_type(Checker* checker, Node* node, const char* name, Type* decl_type,
                                   Type* init_type) {
    if (decl_type && init_type) {
        // Auto-deref Box<T> when declared type is T
        if (!type_assignable(decl_type, init_type) && init_type->kind == TYPE_BOX &&
            type_assignable(decl_type, init_type->as.box.elem) && node->as.var_decl.init) {
            node->as.var_decl.init->is_box_deref = 1;
            init_type                            = init_type->as.box.elem;
        }
        // Both specified - check compatibility
        if (!type_assignable(decl_type, init_type)) {
            check_error_type(checker, node->line, node->column, name, decl_type, init_type);
        }
        return decl_type;
    }
    if (decl_type) {
        return decl_type;
    }
    if (init_type) {
        return init_type;
    }

    check_error(checker, node->line, node->column,
                "Variable '%s' needs type annotation or initializer", name);
    return type_error;
}

static void record_inferred_var_decl_type(Node* node, Type* init_type) {
    if (node->as.var_decl.type || !init_type) {
        return;
    }

    // Store resolved type for codegen when type is inferred from non-literal expressions
    if (init_type->kind == TYPE_STRING && node->as.var_decl.init->type != NODE_STRING_LIT) {
        node->as.var_decl.resolved_type = init_type;
    }

    // Store resolved type for function pointer inference (var fp = some_func)
    if (init_type->kind == TYPE_FUNC) {
        node->as.var_decl.resolved_type = init_type;
    }
}

static void maybe_mark_struct_destructuring_rc(Checker* checker, Node* node, Type* init_type) {
    DestructPattern* pattern = node->as.var_decl.destruct_pattern;
    if (pattern->kind != PATTERN_STRUCT || init_type->kind != TYPE_STRUCT) {
        return;
    }

    // RC tracking for struct destructuring (the temp struct must stay alive)
    Node* init = node->as.var_decl.init;
    if (init->type == NODE_NEW_EXPR) {
        mark_var_decl_rc(node, NULL, init->as.new_expr.resolved_type);
    } else if (init->type == NODE_IDENT) {
        Symbol* src = checker_lookup(checker, init->as.ident.name);
        if (src && src->is_rc) {
            mark_var_decl_rc(node, NULL, src->type);
        }
    } else if (init->type == NODE_CALL || is_module_call_enum_value(init)) {
        mark_var_decl_rc(node, NULL, init_type);
    }
}

static void maybe_mark_var_decl_rc_from_init(Checker* checker, Node* node, Symbol* sym,
                                             Type* var_type) {
    Node* init = node->as.var_decl.init;
    if (!sym || !init) {
        return;
    }

    if (var_type && var_type->kind == TYPE_ENUM && var_type->as.enm.has_rc_fields) {
        mark_var_decl_rc(node, sym, var_type);
    }

    if (init->type == NODE_NEW_EXPR) {
        mark_var_decl_rc(node, sym, init->as.new_expr.resolved_type);
    } else if (init->type == NODE_IDENT && !init->is_box_deref) {
        // Copy from RC var (but not if auto-deref'd from Box — that's a value copy)
        Symbol* src = checker_lookup(checker, init->as.ident.name);
        if (src && src->is_rc) {
            mark_var_decl_rc(node, sym, src->type);
        }
    } else if (init->type == NODE_CALL && var_type) {
        // Store resolved type for codegen type inference
        node->as.var_decl.resolved_type = var_type;
        if (type_is_rc_managed(var_type)) {
            // Function call returning an RC-managed type transfers ownership
            node->as.var_decl.is_rc = 1;
            sym->is_rc              = 1;
        }
    } else if (is_module_call_enum_value(init) && var_type) {
        // Module call (parsed as enum value): store resolved type for codegen
        node->as.var_decl.resolved_type = var_type;
        if (type_is_rc_managed(var_type)) {
            node->as.var_decl.is_rc = 1;
            sym->is_rc              = 1;
        }
    }
}

static void maybe_mark_string_var_decl_rc(Node* node, Symbol* sym, Type* var_type) {
    // All string variables are RC-managed (immortal literals are no-ops for inc/dec)
    if (sym && var_type && var_type->kind == TYPE_STRING && !node->as.var_decl.is_rc) {
        mark_var_decl_rc(node, sym, type_string);
    }
}

static void check_destructuring_var_decl_stmt(Checker* checker, Node* node) {
    DestructPattern* pattern = node->as.var_decl.destruct_pattern;

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
    maybe_mark_struct_destructuring_rc(checker, node, init_type);
}

static void check_normal_var_decl_stmt(Checker* checker, Node* node) {
    const char* name = node->as.var_decl.name;

    // Check for redefinition
    if (checker_lookup_local(checker, name)) {
        check_error(checker, node->line, node->column, "Redefinition of '%s'", name);
        return;
    }

    Type* decl_type = NULL;
    if (node->as.var_decl.type) {
        decl_type = resolve_type(checker, node->as.var_decl.type);
    }

    Type* init_type = check_var_decl_initializer(checker, decl_type, node->as.var_decl.init);

    // Autoboxing: var ^x = expr desugars to Box<T> allocation
    if (node->as.var_decl.is_boxed) {
        if (!node->as.var_decl.init) {
            check_error(checker, node->line, node->column,
                        "Boxed variable declaration requires an initializer");
            return;
        }
        Type* inner = decl_type ? decl_type : init_type;
        if (!inner || inner->kind == TYPE_ERROR) {
            check_error(checker, node->line, node->column,
                        "Cannot determine type for boxed variable '%s'", name);
            return;
        }
        if (decl_type && init_type && !type_assignable(decl_type, init_type)) {
            check_error_type(checker, node->line, node->column, name, decl_type, init_type);
            return;
        }
        Type*   box_type = ensure_box_type(checker, inner);
        Symbol* sym = checker_define(checker, name, SYM_VAR, box_type, node->as.var_decl.is_const,
                                     node->as.var_decl.is_public, checker->modules.current_module);
        mark_var_decl_rc(node, sym, box_type);
        return;
    }

    Type* var_type = resolve_var_decl_type(checker, node, name, decl_type, init_type);

    Symbol* sym = checker_define(checker, name, SYM_VAR, var_type, node->as.var_decl.is_const,
                                 node->as.var_decl.is_public, checker->modules.current_module);

    record_inferred_var_decl_type(node, init_type);
    maybe_mark_var_decl_rc_from_init(checker, node, sym, var_type);
    maybe_mark_string_var_decl_rc(node, sym, var_type);

    // Option<T> without initializer defaults to None
    if (!node->as.var_decl.init && type_is_option(var_type)) {
        // Store resolved type for codegen to emit default None
        node->as.var_decl.resolved_type = var_type;
        // Mark RC if type has RC fields (for scope cleanup)
        if (var_type->as.enm.has_rc_fields) {
            mark_var_decl_rc(node, sym, var_type);
        }
    }
}

// Type-check a variable declaration: handles destructuring, type inference, and RC tracking
static void check_var_decl_stmt(Checker* checker, Node* node) {
    if (node->as.var_decl.destruct_pattern) {
        check_destructuring_var_decl_stmt(checker, node);
        return;
    }

    check_normal_var_decl_stmt(checker, node);
}

// Type-check a for loop: init, condition, post-expression, and body
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

// Type-check a foreach loop: validate range bounds and define loop variable
static void check_foreach_stmt(Checker* checker, Node* node) {
    checker_push_scope(checker); // New scope for loop variable

    Type* loop_type;

    if (node->as.foreach_stmt.collection) {
        // Collection foreach: foreach (const item in vec)
        Type* coll_type = check_expression(checker, node->as.foreach_stmt.collection);

        if (coll_type->kind == TYPE_VEC) {
            loop_type = coll_type->as.vec.elem;
        } else if (coll_type->kind == TYPE_SPAN) {
            loop_type                     = coll_type->as.span.elem;
            node->as.foreach_stmt.is_span = 1;
        } else if (coll_type->kind == TYPE_STRING) {
            loop_type                       = type_char;
            node->as.foreach_stmt.is_string = 1;
        } else if (coll_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.collection->line,
                        node->as.foreach_stmt.collection->column,
                        "Foreach collection must be Vec<T>, Span<T>, or string, got '%s'",
                        type_name(coll_type));
            loop_type = type_int64;
        } else {
            loop_type = type_int64;
        }
    } else {
        // Range foreach: foreach (const i in start..end [by step])
        Type* start_type = check_expression(checker, node->as.foreach_stmt.start);
        Type* end_type   = check_expression(checker, node->as.foreach_stmt.end);

        if (!type_is_integer(start_type) && start_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.start->line,
                        node->as.foreach_stmt.start->column,
                        "Foreach range start must be int, got '%s'", type_name(start_type));
        }

        if (!type_is_integer(end_type) && end_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.end->line, node->as.foreach_stmt.end->column,
                        "Foreach range end must be int, got '%s'", type_name(end_type));
        }

        // Determine loop variable type: prefer end type when start is a default i64 literal
        if (type_is_integer(end_type) &&
            (start_type->kind == TYPE_INT64 || !type_is_integer(start_type))) {
            loop_type = end_type;
        } else if (type_is_integer(start_type)) {
            loop_type = start_type;
        } else {
            loop_type = type_int64;
        }
    }

    node->as.foreach_stmt.resolved_type = loop_type;

    // Add the loop variable as a const (immutable)
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

// Type-check a return statement against the current function's return type
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

        // Auto-deref Box<T> when returning T
        if (actual && actual->kind == TYPE_BOX && !type_assignable(expected, actual) &&
            type_assignable(expected, actual->as.box.elem)) {
            node->as.return_stmt.value->is_box_deref = 1;
            actual                                   = actual->as.box.elem;
        }

        if (!type_assignable(expected, actual)) {
            check_error_type(checker, node->line, node->column, "Return", expected, actual);
        }
    } else if (expected->kind != TYPE_VOID) {
        check_error(checker, node->line, node->column, "Return without value in non-void function");
    }
}

// =============================================================================
// Match statement checking
// =============================================================================

static int type_is_matchable(Type* t) {
    switch (t->kind) {
    case TYPE_INT8:
    case TYPE_INT16:
    case TYPE_INT32:
    case TYPE_INT64:
    case TYPE_UINT8:
    case TYPE_UINT16:
    case TYPE_UINT32:
    case TYPE_UINT64:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_CHAR:
    case TYPE_BOOL:
    case TYPE_STRING:
        return 1;
    default:
        return 0;
    }
}

// Return 1 if node is a literal pattern suitable for value match
static int is_literal_pattern(Node* node) {
    if (!node)
        return 0;
    switch (node->type) {
    case NODE_INT_LIT:
    case NODE_FLOAT_LIT:
    case NODE_STRING_LIT:
    case NODE_CHAR_LIT:
    case NODE_BOOL_LIT:
        return 1;
    case NODE_UNARY:
        // Allow -<numeric_lit>
        if (node->as.unary.op == TOK_MINUS) {
            NodeType inner = node->as.unary.operand->type;
            return inner == NODE_INT_LIT || inner == NODE_FLOAT_LIT;
        }
        return 0;
    default:
        return 0;
    }
}

// Check a match arm body in expression or statement context, unifying arm result types
static int check_match_arm_body(Checker* checker, Node* arm, int is_expr_context,
                                Type** match_value_type, int* had_expr_error) {
    if (is_expr_context) {
        Type* arm_type = check_expression(checker, arm->as.match_arm.body);
        if (arm_type->kind == TYPE_ERROR) {
            *had_expr_error = 1;
            return 0;
        }
        if (!*match_value_type) {
            *match_value_type = arm_type;
        } else if (!type_assignable(*match_value_type, arm_type) ||
                   !type_assignable(arm_type, *match_value_type)) {
            check_error_type(checker, arm->line, arm->column, "Match arm type mismatch",
                             *match_value_type, arm_type);
            *had_expr_error = 1;
        }
    } else {
        check_statement(checker, arm->as.match_arm.body);
    }
    return 1;
}

// Return 1 if any non-wildcard arm references the given enum variant name.
static int match_has_variant_arm(Node* node, const char* variant_name) {
    for (int a = 0; a < node->as.match_stmt.arms.count; a++) {
        Node* arm = node->as.match_stmt.arms.nodes[a];
        if (!arm->as.match_arm.is_wildcard && arm->as.match_arm.variant_name &&
            strcmp(arm->as.match_arm.variant_name, variant_name) == 0) {
            return 1;
        }
    }
    return 0;
}

static Type* check_match_enum(Checker* checker, Node* node, Type* expr_type, int is_expr_context) {
    Type* match_value_type = NULL;
    int   had_expr_error   = 0;

    for (int a = 0; a < node->as.match_stmt.arms.count; a++) {
        Node* arm = node->as.match_stmt.arms.nodes[a];

        if (arm->as.match_arm.is_wildcard) {
            check_match_arm_body(checker, arm, is_expr_context, &match_value_type, &had_expr_error);
            continue;
        }

        // Reject value patterns in enum match
        if (arm->as.match_arm.pattern_expr) {
            check_error(checker, arm->line, arm->column,
                        "Literal pattern cannot be used in match on enum type '%s'",
                        type_name(expr_type));
            continue;
        }

        const char* variant_name = arm->as.match_arm.variant_name;
        int         variant_idx  = type_enum_variant_index(expr_type, variant_name);
        if (variant_idx < 0) {
            check_error(checker, arm->line, arm->column, "'%s' is not a variant of enum '%s'",
                        variant_name, expr_type->as.enm.name);
            continue;
        }

        // If qualified name given, verify it matches the enum type
        if (arm->as.match_arm.enum_name) {
            if (strcmp(arm->as.match_arm.enum_name, expr_type->as.enm.name) != 0) {
                check_error(checker, arm->line, arm->column,
                            "Enum name '%s' does not match match expression type '%s'",
                            arm->as.match_arm.enum_name, expr_type->as.enm.name);
                continue;
            }
        }

        // Check binding count
        int expected_bindings = expr_type->as.enm.variant_type_counts[variant_idx];
        if (arm->as.match_arm.binding_count != expected_bindings) {
            check_error(checker, arm->line, arm->column,
                        "Variant '%s' expects %d binding(s), got %d", variant_name,
                        expected_bindings, arm->as.match_arm.binding_count);
            continue;
        }

        // Push scope, define bindings, check body, pop scope
        checker_push_scope(checker);
        for (int j = 0; j < arm->as.match_arm.binding_count; j++) {
            Type* binding_type = expr_type->as.enm.variant_types[variant_idx][j];
            checker_define(checker, arm->as.match_arm.bindings[j], SYM_VAR, binding_type, 0, 0,
                           NULL);
        }
        check_match_arm_body(checker, arm, is_expr_context, &match_value_type, &had_expr_error);
        checker_pop_scope(checker);
    }

    // Exhaustiveness check: if no wildcard arm, every variant must be covered
    if (!match_stmt_has_wildcard_arm(node)) {
        for (int i = 0; i < expr_type->as.enm.value_count; i++) {
            if (!match_has_variant_arm(node, expr_type->as.enm.value_names[i])) {
                check_error(checker, node->line, node->column,
                            "Match is not exhaustive: missing variant '%s'",
                            expr_type->as.enm.value_names[i]);
            }
        }
    }

    if (!is_expr_context) {
        return type_void;
    }

    if (!match_value_type || had_expr_error) {
        node->as.match_stmt.resolved_value_type = type_error;
        return type_error;
    }
    node->as.match_stmt.resolved_value_type = match_value_type;
    return match_value_type;
}

// Compare two literal pattern nodes for duplicate detection
static int literal_patterns_equal(Node* a, Node* b) {
    if (a->type != b->type)
        return 0;
    switch (a->type) {
    case NODE_INT_LIT:
        return a->as.int_lit.value == b->as.int_lit.value;
    case NODE_FLOAT_LIT:
        return a->as.float_lit.value == b->as.float_lit.value;
    case NODE_STRING_LIT:
        return a->as.string_lit.length == b->as.string_lit.length &&
               memcmp(a->as.string_lit.value, b->as.string_lit.value, a->as.string_lit.length) == 0;
    case NODE_CHAR_LIT:
        return a->as.char_lit.value == b->as.char_lit.value;
    case NODE_BOOL_LIT:
        return a->as.bool_lit.value == b->as.bool_lit.value;
    case NODE_UNARY:
        // Both must be negated numeric literals
        if (a->as.unary.op != b->as.unary.op)
            return 0;
        return literal_patterns_equal(a->as.unary.operand, b->as.unary.operand);
    default:
        return 0;
    }
}

static Type* check_match_value(Checker* checker, Node* node, Type* expr_type, int is_expr_context) {
    Type* match_value_type = NULL;
    int   had_expr_error   = 0;
    int   has_wildcard     = 0;

    for (int a = 0; a < node->as.match_stmt.arms.count; a++) {
        Node* arm = node->as.match_stmt.arms.nodes[a];

        if (arm->as.match_arm.is_wildcard) {
            has_wildcard = 1;
            check_match_arm_body(checker, arm, is_expr_context, &match_value_type, &had_expr_error);
            continue;
        }

        // Reject enum variant patterns in value match
        if (arm->as.match_arm.variant_name) {
            check_error(checker, arm->line, arm->column,
                        "Enum variant pattern cannot be used in match on '%s'",
                        type_name(expr_type));
            continue;
        }

        Node* pat = arm->as.match_arm.pattern_expr;
        if (!pat) {
            check_error(checker, arm->line, arm->column, "Expected literal pattern in value match");
            continue;
        }

        if (!is_literal_pattern(pat)) {
            check_error(checker, arm->line, arm->column, "Match pattern must be a literal value");
            continue;
        }

        // Type-check the pattern expression and verify compatibility
        Type* pat_type = check_expression(checker, pat);
        if (pat_type->kind == TYPE_ERROR)
            continue;

        if (!type_assignable(expr_type, pat_type)) {
            check_error(checker, arm->line, arm->column,
                        "Pattern type '%s' is not compatible with match expression type '%s'",
                        type_name(pat_type), type_name(expr_type));
            continue;
        }

        // Check for duplicate literal patterns
        for (int b = 0; b < a; b++) {
            Node* prev = node->as.match_stmt.arms.nodes[b];
            if (prev->as.match_arm.pattern_expr &&
                literal_patterns_equal(pat, prev->as.match_arm.pattern_expr)) {
                check_error(checker, arm->line, arm->column, "Duplicate match pattern");
                break;
            }
        }

        check_match_arm_body(checker, arm, is_expr_context, &match_value_type, &had_expr_error);
    }

    // Match expressions require a wildcard to guarantee a value is produced
    if (is_expr_context && !has_wildcard) {
        check_error(checker, node->line, node->column,
                    "Match expression requires a wildcard '_' arm");
        return type_error;
    }

    if (is_expr_context) {
        if (!match_value_type || had_expr_error) {
            node->as.match_stmt.resolved_value_type = type_error;
            return type_error;
        }
        node->as.match_stmt.resolved_value_type = match_value_type;
        return match_value_type;
    }

    return type_void;
}

static Type* check_match(Checker* checker, Node* node, int is_expr_context) {
    Type* expr_type = check_expression(checker, node->as.match_stmt.expr);
    if (expr_type->kind == TYPE_ERROR)
        return type_error;

    node->as.match_stmt.resolved_type       = expr_type;
    node->as.match_stmt.resolved_value_type = NULL;

    if (expr_type->kind == TYPE_ENUM) {
        return check_match_enum(checker, node, expr_type, is_expr_context);
    }

    if (type_is_matchable(expr_type)) {
        node->as.match_stmt.is_value_match = 1;
        return check_match_value(checker, node, expr_type, is_expr_context);
    }

    check_error(checker, node->line, node->column,
                "Match expression must be an enum, integer, float, string, char, or bool type, "
                "got '%s'",
                type_name(expr_type));
    return type_error;
}

Type* check_match_expr(Checker* checker, Node* node) {
    return check_match(checker, node, 1);
}

static void check_match_stmt(Checker* checker, Node* node) {
    (void)check_match(checker, node, 0);
}

// =============================================================================
// Statement checking
// =============================================================================

static void check_block_stmt(Checker* checker, Node* node) {
    checker_push_scope(checker);
    for (int i = 0; i < node->as.block.stmts.count; i++) {
        check_statement(checker, node->as.block.stmts.nodes[i]);
    }
    checker_pop_scope(checker);
}

static void check_if_stmt(Checker* checker, Node* node) {
    Type* cond = check_expression(checker, node->as.if_stmt.cond);
    if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
        check_error(checker, node->as.if_stmt.cond->line, node->as.if_stmt.cond->column,
                    "If condition must be bool, got '%s'", type_name(cond));
    }

    check_statement(checker, node->as.if_stmt.then_block);
    if (node->as.if_stmt.else_block) {
        check_statement(checker, node->as.if_stmt.else_block);
    }
}

static void check_while_stmt(Checker* checker, Node* node) {
    Type* cond = check_expression(checker, node->as.while_stmt.cond);
    if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
        check_error(checker, node->as.while_stmt.cond->line, node->as.while_stmt.cond->column,
                    "While condition must be bool, got '%s'", type_name(cond));
    }

    int was_in_loop  = checker->in_loop;
    checker->in_loop = 1;
    check_statement(checker, node->as.while_stmt.body);
    checker->in_loop = was_in_loop;
}

static void check_break_stmt(Checker* checker, Node* node) {
    if (!checker->in_loop) {
        check_error(checker, node->line, node->column, "Break outside of loop");
    }
}

static void check_continue_stmt(Checker* checker, Node* node) {
    if (!checker->in_loop) {
        check_error(checker, node->line, node->column, "Continue outside of loop");
    }
}

static void check_defer_stmt(Checker* checker, Node* node) {
    if (!checker->current_func_return) {
        check_error(checker, node->line, node->column, "Defer outside of function");
        return;
    }
    check_statement(checker, node->as.defer_stmt.stmt);
}

static int check_if_let_variant(Checker* checker, Node* node, Type* expr_type,
                                int* variant_idx_out) {
    const char* variant_name = node->as.if_let_stmt.variant_name;
    int         variant_idx  = type_enum_variant_index(expr_type, variant_name);
    if (variant_idx < 0) {
        check_error(checker, node->line, node->column, "'%s' is not a variant of enum '%s'",
                    variant_name, expr_type->as.enm.name);
        return 0;
    }

    // If qualified name given, verify it matches
    if (node->as.if_let_stmt.enum_name &&
        strcmp(node->as.if_let_stmt.enum_name, expr_type->as.enm.name) != 0) {
        check_error(checker, node->line, node->column,
                    "Enum name '%s' does not match expression type '%s'",
                    node->as.if_let_stmt.enum_name, expr_type->as.enm.name);
        return 0;
    }

    *variant_idx_out = variant_idx;
    return 1;
}

static int check_if_let_binding_count(Checker* checker, Node* node, Type* expr_type,
                                      int variant_idx) {
    // Check binding count: allow bare check (0 bindings) or exact match
    int         expected_bindings = expr_type->as.enm.variant_type_counts[variant_idx];
    const char* variant_name      = node->as.if_let_stmt.variant_name;
    if (node->as.if_let_stmt.binding_count != 0 &&
        node->as.if_let_stmt.binding_count != expected_bindings) {
        check_error(checker, node->line, node->column, "Variant '%s' expects %d binding(s), got %d",
                    variant_name, expected_bindings, node->as.if_let_stmt.binding_count);
        return 0;
    }
    return 1;
}

static void check_if_let_extra_cond(Checker* checker, Node* node) {
    // Type-check optional && condition (bindings are in scope)
    if (node->as.if_let_stmt.extra_cond) {
        Type* cond_type = check_expression(checker, node->as.if_let_stmt.extra_cond);
        if (cond_type->kind != TYPE_ERROR && cond_type->kind != TYPE_BOOL) {
            check_error(checker, node->as.if_let_stmt.extra_cond->line,
                        node->as.if_let_stmt.extra_cond->column,
                        "'is' pattern '&&' condition must be bool, got '%s'", type_name(cond_type));
        }
    }
}

static void check_if_let_then_scope(Checker* checker, Node* node, Type* expr_type,
                                    int variant_idx) {
    checker_push_scope(checker);
    for (int j = 0; j < node->as.if_let_stmt.binding_count; j++) {
        Type* binding_type = expr_type->as.enm.variant_types[variant_idx][j];
        checker_define(checker, node->as.if_let_stmt.bindings[j], SYM_VAR, binding_type, 0, 0,
                       NULL);
    }

    check_if_let_extra_cond(checker, node);
    check_statement(checker, node->as.if_let_stmt.then_block);
    checker_pop_scope(checker);
}

static void check_if_let_stmt(Checker* checker, Node* node) {
    // Type-check expression
    Type* expr_type = check_expression(checker, node->as.if_let_stmt.expr);
    if (expr_type->kind == TYPE_ERROR) {
        return;
    }

    // Must be an enum type
    if (expr_type->kind != TYPE_ENUM) {
        check_error(checker, node->line, node->column,
                    "'is' pattern requires an enum type, got '%s'", type_name(expr_type));
        return;
    }

    node->as.if_let_stmt.resolved_type = expr_type;

    int variant_idx = -1;
    if (!check_if_let_variant(checker, node, expr_type, &variant_idx)) {
        return;
    }
    if (!check_if_let_binding_count(checker, node, expr_type, variant_idx)) {
        return;
    }

    check_if_let_then_scope(checker, node, expr_type, variant_idx);

    // Check else_block (no bindings in scope)
    if (node->as.if_let_stmt.else_block) {
        check_statement(checker, node->as.if_let_stmt.else_block);
    }
}

// Dispatch statement type-checking based on node type
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
        check_block_stmt(checker, node);
        break;

    case NODE_IF:
        check_if_stmt(checker, node);
        break;

    case NODE_WHILE:
        check_while_stmt(checker, node);
        break;

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
        check_break_stmt(checker, node);
        break;

    case NODE_CONTINUE:
        check_continue_stmt(checker, node);
        break;

    case NODE_DEFER:
        check_defer_stmt(checker, node);
        break;

    case NODE_MATCH:
        check_match_stmt(checker, node);
        break;

    case NODE_IF_LET:
        check_if_let_stmt(checker, node);
        break;

    default:
        check_error(checker, node->line, node->column, "Unknown statement type %d", node->type);
        break;
    }
}

// =============================================================================
// Declaration checking
// =============================================================================

// Build a TYPE_FUNC from a function declaration's parameter and return type annotations
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

// Process an extern module block: register each declared function in the symbol table
static void check_extern_module_decl(Checker* checker, Node* node) {
    for (int i = 0; i < node->as.extern_module.decls.count; i++) {
        Node* decl = node->as.extern_module.decls.nodes[i];
        if (decl->type == NODE_FUNC_DECL) {
            func_decl_node* fdn = &decl->as.func_decl;
            if (fdn->return_type == NULL) {
                check_error(checker, decl->line, decl->column,
                            "Extern function '%s' must have an explicit return type", fdn->name);
                continue;
            }
            Type* func_type = get_function_type(checker, decl);

            checker_define(checker, fdn->name, SYM_FUNC, func_type, 0, fdn->is_public,
                           checker->modules.current_module);
        }
    }
}

static int has_receiver_type_var(char** names, int count, const char* name) {
    for (int i = 0; i < count; i++) {
        if (strcmp(names[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

static int is_receiver_type_variable(Checker* checker, const char* name) {
    if (type_builtin_from_name(name)) {
        return 0;
    }
    Symbol* sym = checker_lookup_any(checker, name);
    return !(sym && sym->kind == SYM_TYPE);
}

static void collect_receiver_type_vars(Checker* checker, Node* node, char*** names, int* count,
                                       int* capacity) {
    if (!node) {
        return;
    }

    switch (node->type) {
    case NODE_IDENT: {
        const char* name = node->as.ident.name;
        if (!is_receiver_type_variable(checker, name) ||
            has_receiver_type_var(*names, *count, name)) {
            return;
        }
        if (*count >= *capacity) {
            *capacity = (*capacity == 0) ? 4 : (*capacity * 2);
            *names    = xrealloc(*names, (*capacity) * sizeof(char*));
        }
        (*names)[(*count)++] = xstrdup(name);
        return;
    }
    case NODE_GENERIC_TYPE:
        for (int i = 0; i < node->as.generic_type.type_args.count; i++) {
            collect_receiver_type_vars(checker, node->as.generic_type.type_args.nodes[i], names,
                                       count, capacity);
        }
        return;
    case NODE_ARRAY_TYPE:
        collect_receiver_type_vars(checker, node->as.array_type.elem_type, names, count, capacity);
        return;
    case NODE_FUNC_TYPE:
        for (int i = 0; i < node->as.func_type.param_types.count; i++) {
            collect_receiver_type_vars(checker, node->as.func_type.param_types.nodes[i], names,
                                       count, capacity);
        }
        collect_receiver_type_vars(checker, node->as.func_type.return_type, names, count, capacity);
        return;
    case NODE_TUPLE_TYPE:
        for (int i = 0; i < node->as.tuple_type.elem_types.count; i++) {
            collect_receiver_type_vars(checker, node->as.tuple_type.elem_types.nodes[i], names,
                                       count, capacity);
        }
        return;
    default:
        return;
    }
}

static int try_register_generic_func_or_method(Checker* checker, Node* node) {
    func_decl_node* fdn         = &node->as.func_decl;
    const char*     name        = fdn->name;
    const char*     receiver    = fdn->receiver_type;
    int             is_method   = (receiver != NULL);
    int             has_typearg = fdn->receiver_type_args.count > 0;

    // Generic free function: func identity<T>(x: T) -> T
    if (!is_method && fdn->type_param_count > 0) {
        register_generic_func_def(checker, name, fdn->type_params, fdn->type_param_bounds,
                                  fdn->type_param_count, node);
        return 1;
    }

    // Method-level generic: func (Vec<T>) map<K>(...) -> Vec<K>
    // Must be checked BEFORE the plain generic receiver method branch
    if (is_method && has_typearg && fdn->type_param_count > 0) {
        GenericDef* def = lookup_generic_def(checker, receiver);
        if (!def) {
            check_error(checker, node->line, node->column, "Unknown generic type '%s'", receiver);
            return 1;
        }
        if (fdn->receiver_type_args.count != def->type_param_count) {
            check_error(checker, node->line, node->column,
                        "Generic type '%s' expects %d type parameters, got %d", receiver,
                        def->type_param_count, fdn->receiver_type_args.count);
            return 1;
        }
        // Build combined type params: receiver pattern bindings + method's own type params.
        // Example: func (Pair<i32, Box<T>>) map<U>(...) => ["T", "U"].
        char** recv_params   = NULL;
        int    recv_count    = 0;
        int    recv_capacity = 0;
        for (int i = 0; i < fdn->receiver_type_args.count; i++) {
            collect_receiver_type_vars(checker, fdn->receiver_type_args.nodes[i], &recv_params,
                                       &recv_count, &recv_capacity);
        }

        int    method_count = fdn->type_param_count;
        int    combined     = recv_count + method_count;
        char** params       = xmalloc(combined * sizeof(char*));
        char** bounds       = xmalloc(combined * sizeof(char*));
        for (int i = 0; i < recv_count; i++) {
            params[i] = recv_params[i];
            // Receiver generic bounds are enforced when the receiver type is instantiated.
            bounds[i] = NULL;
        }
        for (int i = 0; i < method_count; i++) {
            params[recv_count + i] = fdn->type_params[i];
            bounds[recv_count + i] = fdn->type_param_bounds[i];
        }
        register_generic_method_func_def(checker, receiver, name, params, bounds, combined,
                                         recv_count, node);
        for (int i = 0; i < recv_count; i++) {
            free(recv_params[i]);
        }
        free(recv_params);
        free(params);
        free(bounds);
        return 1;
    }

    // Generic receiver method: func (Box<T>) get() -> T
    if (is_method && has_typearg) {
        GenericDef* def = lookup_generic_def(checker, receiver);
        if (!def) {
            check_error(checker, node->line, node->column, "Unknown generic type '%s'", receiver);
            return 1;
        }
        if (fdn->receiver_type_args.count != def->type_param_count) {
            check_error(checker, node->line, node->column,
                        "Generic type '%s' expects %d type parameters, got %d", receiver,
                        def->type_param_count, fdn->receiver_type_args.count);
            return 1;
        }
        register_generic_method(def, node);
        return 1;
    }

    return 0;
}

static char* get_mangled_func_name(const char* name, const char* receiver_type) {
    if (!receiver_type) {
        return xstrdup(name);
    }

    size_t len = strlen(receiver_type) + 1 + strlen(name) + 1;
    char*  out = xmalloc(len);
    snprintf(out, len, "%s_%s", receiver_type, name);
    return out;
}

static void check_main_func_signature(Checker* checker, Node* node) {
    func_decl_node* fdn = &node->as.func_decl;
    if (fdn->params.count != 0) {
        check_error(checker, node->line, node->column, "main function must not have parameters");
    }

    if (!fdn->return_type) {
        check_error(checker, node->line, node->column, "main function must have return type i32");
        return;
    }

    Type* ret_type = resolve_type(checker, fdn->return_type);
    if (ret_type->kind != TYPE_INT32) {
        check_error(checker, node->line, node->column, "main function must have return type i32");
    }
}

static int check_function_redefinition(Checker* checker, Node* node, const char* mangled_name) {
    Symbol* existing = checker_lookup(checker, mangled_name);
    if (existing && !is_prelude_symbol(existing)) {
        check_error(checker, node->line, node->column, "Redefinition of '%s'", mangled_name);
        return 0;
    }
    return 1;
}

static void add_struct_method(Type* st, const char* name, Type* func_type, int is_const) {
    int n = st->as.struc.method_count;

    st->as.struc.method_names    = xrealloc(st->as.struc.method_names, (n + 1) * sizeof(char*));
    st->as.struc.method_types    = xrealloc(st->as.struc.method_types, (n + 1) * sizeof(Type*));
    st->as.struc.method_is_const = xrealloc(st->as.struc.method_is_const, (n + 1) * sizeof(int));

    st->as.struc.method_names[n]    = xstrdup(name);
    st->as.struc.method_types[n]    = func_type;
    st->as.struc.method_is_const[n] = is_const;
    st->as.struc.method_count       = n + 1;
}

static void add_enum_method(Type* et, const char* name, Type* func_type, int is_const) {
    int n = et->as.enm.method_count;

    et->as.enm.method_names    = xrealloc(et->as.enm.method_names, (n + 1) * sizeof(char*));
    et->as.enm.method_types    = xrealloc(et->as.enm.method_types, (n + 1) * sizeof(Type*));
    et->as.enm.method_is_const = xrealloc(et->as.enm.method_is_const, (n + 1) * sizeof(int));

    et->as.enm.method_names[n]    = xstrdup(name);
    et->as.enm.method_types[n]    = func_type;
    et->as.enm.method_is_const[n] = is_const;
    et->as.enm.method_count       = n + 1;
}

static void register_method_on_receiver(Checker* checker, Node* node, const char* receiver_type,
                                        const char* name, Type* func_type, int is_const) {
    Symbol* sym = checker_lookup(checker, receiver_type);
    if (sym && sym->kind == SYM_TYPE && sym->type->kind == TYPE_STRUCT) {
        add_struct_method(sym->type, name, func_type, is_const);
    } else if (sym && sym->kind == SYM_TYPE && sym->type->kind == TYPE_ENUM) {
        add_enum_method(sym->type, name, func_type, is_const);
    } else if (type_builtin_from_name(receiver_type)) {
        // Register method on a primitive type.
        VEC_GROW(checker->traits.primitive_methods, checker->traits.primitive_method_count,
                 checker->traits.primitive_method_capacity);
        PrimitiveMethod* pm =
            &checker->traits.primitive_methods[checker->traits.primitive_method_count++];
        pm->type_name   = xstrdup(receiver_type);
        pm->method_name = xstrdup(name);
        pm->method_type = func_type;
        pm->is_const    = is_const;
    } else {
        check_error(checker, node->line, node->column, "Unknown receiver type '%s'", receiver_type);
    }
}

static void define_method_self_symbol(Checker* checker, const char* receiver_type, int is_const) {
    Symbol* sym = checker_lookup(checker, receiver_type);
    if (sym && sym->kind == SYM_TYPE) {
        checker_define(checker, "self", SYM_VAR, sym->type, is_const, 0, NULL);
        return;
    }

    Type* builtin = type_builtin_from_name(receiver_type);
    if (builtin) {
        checker_define(checker, "self", SYM_VAR, builtin, is_const, 0, NULL);
    }
}

static void define_func_params_in_scope(Checker* checker, func_decl_node* fdn, Type* func_type) {
    for (int i = 0; i < func_type->as.func.param_count; i++) {
        Node* param = fdn->params.nodes[i];
        Type* ptype = func_type->as.func.param_types[i];
        if (!checker_define(checker, param->as.param.name, SYM_VAR, ptype, param->as.param.is_const,
                            0, NULL)) {
            check_error(checker, param->line, param->column, "Duplicate parameter name '%s'",
                        param->as.param.name);
        }
    }
}

static void check_function_body(Checker* checker, Node* body) {
    if (!body) {
        return;
    }
    for (int i = 0; i < body->as.block.stmts.count; i++) {
        check_statement(checker, body->as.block.stmts.nodes[i]);
    }
}

// Type-check a function declaration: signature, parameters, body, and method registration
static void check_func_decl(Checker* checker, Node* node) {
    func_decl_node* fdn = &node->as.func_decl;

    const char* name          = fdn->name;
    const char* receiver_type = fdn->receiver_type;
    int         is_method     = (receiver_type != NULL);

    if (try_register_generic_func_or_method(checker, node)) {
        return;
    }

    char* mangled_name = get_mangled_func_name(name, receiver_type);

    // if main function, ensure correct signature
    if (!is_method && strcmp(name, "main") == 0) {
        check_main_func_signature(checker, node);
    }

    if (!check_function_redefinition(checker, node, mangled_name)) {
        free(mangled_name);
        return;
    }

    Type* func_type = get_function_type(checker, node);

    // Pre-declare function for recursion
    checker_define(checker, mangled_name, SYM_FUNC, func_type, 1, fdn->is_public,
                   checker->modules.current_module);

    // For methods, also register the method on the struct type (or primitive)
    if (is_method) {
        register_method_on_receiver(checker, node, receiver_type, name, func_type,
                                    fdn->receiver_is_const);
    }

    // Enter function scope
    checker_push_scope(checker);
    Type* old_return             = checker->current_func_return;
    checker->current_func_return = func_type->as.func.return_type;

    // Set this function's accessible modules for visibility checking
    char** old_accessible_modules               = checker->modules.current_accessible_modules;
    int    old_accessible_modules_count         = checker->modules.current_accessible_modules_count;
    checker->modules.current_accessible_modules = fdn->accessible_modules;
    checker->modules.current_accessible_modules_count = fdn->accessible_modules_count;

    // For methods, inject 'self' into scope and set private field access context.
    const char* old_receiver = checker->current_method_receiver;
    if (is_method) {
        define_method_self_symbol(checker, receiver_type, fdn->receiver_is_const);
        checker->current_method_receiver = receiver_type;
    }

    define_func_params_in_scope(checker, fdn, func_type);
    check_function_body(checker, fdn->body);

    // Restore previous accessible modules context
    checker->modules.current_accessible_modules       = old_accessible_modules;
    checker->modules.current_accessible_modules_count = old_accessible_modules_count;

    checker->current_method_receiver = old_receiver;
    checker->current_func_return     = old_return;
    checker_pop_scope(checker);
    free(mangled_name);
}

// Type-check a struct declaration: resolve fields, detect RC fields, or register generic template
static int is_prelude_symbol(Symbol* sym) {
    return sym && sym->source_module && strcmp(sym->source_module, "prelude") == 0;
}

static void check_struct_decl(Checker* checker, Node* node) {
    const char* name = node->as.struct_decl.name;

    // Check for redefinition (allow shadowing prelude symbols)
    Symbol* existing = checker_lookup(checker, name);
    if (existing && !is_prelude_symbol(existing)) {
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

    struct_type->as.struc.field_count      = field_count;
    struct_type->as.struc.field_names      = xmalloc(field_count * sizeof(char*));
    struct_type->as.struc.field_types      = xmalloc(field_count * sizeof(Type*));
    struct_type->as.struc.field_is_const   = xmalloc(field_count * sizeof(int));
    struct_type->as.struc.field_is_private = xmalloc(field_count * sizeof(int));

    for (int i = 0; i < field_count; i++) {
        Node* field                               = node->as.struct_decl.fields.nodes[i];
        struct_type->as.struc.field_names[i]      = xstrdup(field->as.field.name);
        struct_type->as.struc.field_types[i]      = resolve_type(checker, field->as.field.type);
        struct_type->as.struc.field_is_const[i]   = field->as.field.is_const;
        struct_type->as.struc.field_is_private[i] = field->as.field.is_private;
    }

    // Check if any field is an RC-managed type (struct, Vec, or enum with RC fields)
    for (int i = 0; i < field_count; i++) {
        Type* ftype = struct_type->as.struc.field_types[i];
        if (type_is_rc_managed(ftype)) {
            struct_type->as.struc.has_rc_fields = 1;
            break;
        }
    }

    checker_define(checker, name, SYM_TYPE, struct_type, 0, node->as.struct_decl.is_public,
                   checker->modules.current_module);
}

// Type-check an enum declaration: resolve variant types or register generic template
static void check_enum_decl(Checker* checker, Node* node) {
    const char* name        = node->as.enum_decl.name;
    int         value_count = node->as.enum_decl.values.count;

    // Check for redefinition (allow shadowing prelude symbols)
    Symbol* existing = checker_lookup(checker, name);
    if (existing && !is_prelude_symbol(existing)) {
        check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
        return;
    }

    // Data enums (any payload variant) do not support explicit integer assignments.
    int   has_data_variant = 0;
    Node* explicit_variant = NULL;
    for (int i = 0; i < value_count; i++) {
        Node* val = node->as.enum_decl.values.nodes[i];
        if (val->as.enum_variant.types.count > 0)
            has_data_variant = 1;
        if (val->as.enum_variant.has_explicit_value && !explicit_variant)
            explicit_variant = val;
    }
    if (has_data_variant && explicit_variant) {
        check_error(checker, explicit_variant->line, explicit_variant->column,
                    "Cannot assign explicit values in enums with data variants");
        return;
    }

    // Check if this is a generic enum definition
    if (node->as.enum_decl.type_param_count > 0) {
        register_generic_def(checker, name, node->as.enum_decl.type_params,
                             node->as.enum_decl.type_param_bounds,
                             node->as.enum_decl.type_param_count, node);
        return;
    }

    Type* enum_type = type_enum(name);

    enum_type->as.enm.value_count         = value_count;
    enum_type->as.enm.value_names         = xmalloc(value_count * sizeof(char*));
    enum_type->as.enm.variant_types       = xmalloc(value_count * sizeof(Type**));
    enum_type->as.enm.variant_type_counts = xmalloc(value_count * sizeof(int));

    checker_define(checker, name, SYM_TYPE, enum_type, 0, node->as.enum_decl.is_public,
                   checker->modules.current_module);

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
                if (type_is_rc_managed(resolved)) {
                    enum_type->as.enm.has_rc_fields = 1;
                }
            }
        } else {
            enum_type->as.enm.variant_types[i] = NULL;
        }
    }
}

// Type-check a trait declaration: build TYPE_TRAIT with method signatures
static void check_trait_decl(Checker* checker, Node* node) {
    const char* name = node->as.trait_decl.name;

    // Check for redefinition (allow shadowing prelude symbols)
    Symbol* existing = checker_lookup(checker, name);
    if (existing && !is_prelude_symbol(existing)) {
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

    // Set Self to a generic param placeholder so trait method types store
    // TYPE_GENERIC_PARAM("Self")
    checker->self_type = type_generic_param("Self");

    for (int i = 0; i < method_count; i++) {
        Node* method                            = node->as.trait_decl.methods.nodes[i];
        trait_type->as.trait.method_names[i]    = xstrdup(method->as.func_decl.name);
        trait_type->as.trait.method_types[i]    = get_function_type(checker, method);
        trait_type->as.trait.method_is_const[i] = method->as.func_decl.receiver_is_const;
    }

    checker->self_type = NULL;

    checker_define(checker, name, SYM_TYPE, trait_type, 0, node->as.trait_decl.is_public,
                   checker->modules.current_module);
}

// Type-check a type alias: resolve target type or register generic alias template
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
                   checker->modules.current_module);
}

// Substitute TYPE_GENERIC_PARAM("Self") with the concrete implementing type
static Type* substitute_self(Type* type, Type* concrete) {
    if (type->kind == TYPE_GENERIC_PARAM && strcmp(type->as.generic_param.name, "Self") == 0)
        return concrete;
    return type;
}

static int find_trait_method_index(Type* trait_type, const char* method_name) {
    for (int i = 0; i < trait_type->as.trait.method_count; i++) {
        if (strcmp(trait_type->as.trait.method_names[i], method_name) == 0) {
            return i;
        }
    }
    return -1;
}

static void add_deferred_trait_check(Checker* checker, const char* type_name,
                                     const char* method_name, Type* expected_type, int is_const,
                                     int is_generic, int line, int col) {
    VEC_GROW(checker->traits.deferred_checks, checker->traits.deferred_check_count,
             checker->traits.deferred_check_capacity);
    DeferredTraitCheck* dc =
        &checker->traits.deferred_checks[checker->traits.deferred_check_count++];
    dc->type_name     = xstrdup(type_name);
    dc->method_name   = xstrdup(method_name);
    dc->expected_type = expected_type;
    dc->is_const      = is_const;
    dc->is_generic    = is_generic;
    dc->line          = line;
    dc->col           = col;
}

static int check_trait_method_signature(Checker* checker, Node* method, const char* trait_name,
                                        Type* trait_type, int trait_method_idx,
                                        Type* impl_func_type) {
    Type* trait_func_type = trait_type->as.trait.method_types[trait_method_idx];
    Type* trait_ret = substitute_self(trait_func_type->as.func.return_type, checker->self_type);

    if (!type_equals(impl_func_type->as.func.return_type, trait_ret)) {
        check_error(checker, method->line, method->column,
                    "Method '%s' return type mismatch: trait '%s' expects '%s', got '%s'",
                    method->as.func_decl.name, trait_name, type_name(trait_ret),
                    type_name(impl_func_type->as.func.return_type));
        return 0;
    }

    if (impl_func_type->as.func.param_count != trait_func_type->as.func.param_count) {
        check_error(checker, method->line, method->column,
                    "Method '%s' parameter count mismatch: trait '%s' expects %d, got %d",
                    method->as.func_decl.name, trait_name, trait_func_type->as.func.param_count,
                    impl_func_type->as.func.param_count);
        return 0;
    }

    for (int p = 0; p < trait_func_type->as.func.param_count; p++) {
        Type* trait_param =
            substitute_self(trait_func_type->as.func.param_types[p], checker->self_type);
        if (!type_equals(impl_func_type->as.func.param_types[p], trait_param)) {
            check_error(checker, method->line, method->column,
                        "Method '%s' parameter %d type mismatch: trait '%s' expects '%s', got '%s'",
                        method->as.func_decl.name, p + 1, trait_name, type_name(trait_param),
                        type_name(impl_func_type->as.func.param_types[p]));
        }
    }

    return 1;
}

static int resolve_trait_impl_target(Checker* checker, Node* node, const char* type_name_str,
                                     Symbol** out_type_sym, int* out_is_generic,
                                     int* out_is_primitive) {
    Symbol* type_sym     = checker_lookup(checker, type_name_str);
    int     is_generic   = 0;
    int     is_primitive = 0;

    if (!type_sym || type_sym->kind != SYM_TYPE ||
        (type_sym->type->kind != TYPE_STRUCT && type_sym->type->kind != TYPE_ENUM)) {
        if (lookup_generic_def(checker, type_name_str)) {
            is_generic = 1;
        } else if (type_builtin_from_name(type_name_str)) {
            is_primitive = 1;
        } else {
            check_error(checker, node->line, node->column,
                        "Cannot implement trait for unknown type '%s'", type_name_str);
            return 0;
        }
    }

    *out_type_sym     = type_sym;
    *out_is_generic   = is_generic;
    *out_is_primitive = is_primitive;
    return 1;
}

static void set_self_type_for_trait_impl(Checker* checker, const char* type_name_str,
                                         Symbol* type_sym, int is_generic, int is_primitive) {
    if (is_primitive) {
        checker->self_type = type_builtin_from_name(type_name_str);
    } else if (!is_generic) {
        checker->self_type = type_sym->type;
    }
}

static void check_trait_impl_method(Checker* checker, Node* method, Type* trait_type,
                                    const char* trait_name, const char* type_name_str,
                                    int is_generic) {
    const char* method_name      = method->as.func_decl.name;
    int         trait_method_idx = find_trait_method_index(trait_type, method_name);

    if (trait_method_idx < 0) {
        check_error(checker, method->line, method->column,
                    "Method '%s' is not declared in trait '%s'", method_name, trait_name);
        return;
    }

    int trait_is_const = trait_type->as.trait.method_is_const[trait_method_idx];
    int impl_is_const  = method->as.func_decl.receiver_is_const;
    if (impl_is_const != trait_is_const) {
        check_error(checker, method->line, method->column,
                    "Method '%s' receiver mutability mismatch: trait '%s' declares '%s', "
                    "impl provides '%s'",
                    method_name, trait_name, trait_is_const ? "const func" : "func",
                    impl_is_const ? "const func" : "func");
        return;
    }

    if (is_generic) {
        if (method->as.func_decl.body == NULL) {
            // Full signature validation is deferred to concrete instantiations.
            add_deferred_trait_check(checker, type_name_str, method_name, NULL, impl_is_const, 1,
                                     method->line, method->column);
            return;
        }
        // For generic structs, receiver methods register through the generic method path.
        check_decl(checker, method);
        return;
    }

    Type* impl_func_type = get_function_type(checker, method);
    if (!check_trait_method_signature(checker, method, trait_name, trait_type, trait_method_idx,
                                      impl_func_type)) {
        return;
    }

    if (method->as.func_decl.body == NULL) {
        add_deferred_trait_check(checker, type_name_str, method_name, impl_func_type, impl_is_const,
                                 0, method->line, method->column);
        return;
    }

    // Process the method as a regular func_decl (registers on struct/primitive, checks body).
    check_decl(checker, method);
}

static int impl_decl_has_method(Node* impl_decl, const char* method_name) {
    for (int i = 0; i < impl_decl->as.impl_decl.methods.count; i++) {
        if (strcmp(impl_decl->as.impl_decl.methods.nodes[i]->as.func_decl.name, method_name) == 0) {
            return 1;
        }
    }
    return 0;
}

static void check_required_trait_methods(Checker* checker, Node* impl_decl, Type* trait_type,
                                         const char* trait_name) {
    for (int i = 0; i < trait_type->as.trait.method_count; i++) {
        const char* required = trait_type->as.trait.method_names[i];
        if (!impl_decl_has_method(impl_decl, required)) {
            check_error(checker, impl_decl->line, impl_decl->column,
                        "Missing required method '%s' from trait '%s'", required, trait_name);
        }
    }
}

static void record_trait_impl(Checker* checker, const char* trait_name, const char* type_name_str) {
    VEC_GROW(checker->traits.impls, checker->traits.impl_count, checker->traits.impl_capacity);
    TraitImpl* impl  = &checker->traits.impls[checker->traits.impl_count++];
    impl->trait_name = xstrdup(trait_name);
    impl->type_name  = xstrdup(type_name_str);
}

static void apply_trait_impl_flags(Checker* checker, Node* node, const char* trait_name,
                                   const char* type_name_str, Symbol* type_sym, int is_generic,
                                   int is_primitive) {
    if (strcmp(trait_name, "Drop") == 0 && !is_generic && !is_primitive) {
        if (type_sym->type->kind == TYPE_STRUCT) {
            type_sym->type->as.struc.has_drop = 1;
        } else if (type_sym->type->kind == TYPE_ENUM) {
            // Drop on enums is not yet supported — but don't access .as.struc
            check_error(checker, node->line, node->column,
                        "Drop trait is not supported for enum types");
        }
    }

    if (strcmp(trait_name, "Eq") == 0 && !is_generic && !is_primitive) {
        if (type_sym->type->kind != TYPE_STRUCT) {
            return;
        }
        type_sym->type->as.struc.has_eq = 1;
        // All struct-typed fields must also implement Eq.
        Type* stype = type_sym->type;
        for (int i = 0; i < stype->as.struc.field_count; i++) {
            Type* ft = stype->as.struc.field_types[i];
            if (ft->kind == TYPE_STRUCT && !ft->as.struc.has_eq) {
                check_error(
                    checker, node->line, node->column,
                    "Cannot implement Eq for '%s': field '%s' of type '%s' does not implement Eq",
                    type_name_str, stype->as.struc.field_names[i], ft->as.struc.name);
            }
        }
    }
}

// Type-check an inherent impl block (no trait): impl Type { methods }
static void check_inherent_impl_decl(Checker* checker, Node* node) {
    const char* type_name_str = node->as.impl_decl.type_name;

    // Look up the target type
    Symbol*     type_sym    = checker_lookup(checker, type_name_str);
    GenericDef* generic_def = NULL;
    int         is_generic  = 0;
    if (!type_sym || type_sym->kind != SYM_TYPE ||
        (type_sym->type->kind != TYPE_STRUCT && type_sym->type->kind != TYPE_ENUM)) {
        generic_def = lookup_generic_def(checker, type_name_str);
        if (generic_def) {
            is_generic = 1;
        } else {
            check_error(checker, node->line, node->column,
                        "Cannot implement methods for unknown type '%s'", type_name_str);
            return;
        }
    }

    // Set Self for method body checking
    if (!is_generic)
        checker->self_type = type_sym->type;

    for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
        Node*           method      = node->as.impl_decl.methods.nodes[i];
        const char*     method_name = method->as.func_decl.name;
        func_decl_node* fdn         = &method->as.func_decl;

        // Validate init method constraints
        if (strcmp(method_name, "init") == 0) {
            if (fdn->return_type != NULL) {
                check_error(checker, method->line, method->column,
                            "init must not have a return type");
                continue;
            }
            if (!is_generic && type_sym->type->kind == TYPE_STRUCT) {
                if (type_sym->type->as.struc.has_init) {
                    check_error(checker, method->line, method->column,
                                "Type '%s' already has an init method", type_name_str);
                    continue;
                }
            }
        }

        if (is_generic) {
            check_decl(checker, method);

            // Set has_init on the generic def
            if (strcmp(method_name, "init") == 0) {
                generic_def->has_init = 1;
            }
        } else {
            check_decl(checker, method);

            // Set has_init flag after successful check_decl
            if (strcmp(method_name, "init") == 0 && type_sym->type->kind == TYPE_STRUCT) {
                type_sym->type->as.struc.has_init = 1;
            }
        }
    }

    checker->self_type = NULL;
}

// Type-check an impl block: verify methods match trait signatures and register trait impl
static void check_impl_decl(Checker* checker, Node* node) {
    const char* trait_name    = node->as.impl_decl.trait_name;
    const char* type_name_str = node->as.impl_decl.type_name;

    // Handle inherent impl blocks (no trait)
    if (trait_name == NULL) {
        check_inherent_impl_decl(checker, node);
        return;
    }

    // Look up the trait
    Symbol* trait_sym = checker_lookup(checker, trait_name);
    if (!trait_sym || trait_sym->kind != SYM_TYPE || trait_sym->type->kind != TYPE_TRAIT) {
        check_error(checker, node->line, node->column, "Unknown trait '%s'", trait_name);
        return;
    }
    Type* trait_type = trait_sym->type;

    // Look up the target type.
    Symbol* type_sym     = NULL;
    int     is_generic   = 0;
    int     is_primitive = 0;
    if (!resolve_trait_impl_target(checker, node, type_name_str, &type_sym, &is_generic,
                                   &is_primitive)) {
        return;
    }

    // Set Self to the concrete implementing type for method body checking.
    set_self_type_for_trait_impl(checker, type_name_str, type_sym, is_generic, is_primitive);

    // Process each method in the impl block.
    for (int i = 0; i < node->as.impl_decl.methods.count; i++) {
        check_trait_impl_method(checker, node->as.impl_decl.methods.nodes[i], trait_type,
                                trait_name, type_name_str, is_generic);
    }

    check_required_trait_methods(checker, node, trait_type, trait_name);

    record_trait_impl(checker, trait_name, type_name_str);
    apply_trait_impl_flags(checker, node, trait_name, type_name_str, type_sym, is_generic,
                           is_primitive);

    checker->self_type = NULL;
}

// Insert an unqualified alias for a module symbol into the current scope.
static void insert_use_alias(Checker* checker, Symbol* sym, const char* sym_name) {
    Scope*       scope = checker->scope;
    unsigned int index = hash_string(sym_name) % scope->size;

    Symbol* alias         = xcalloc(1, sizeof(Symbol));
    alias->kind           = sym->kind;
    alias->name           = xstrdup(sym_name);
    alias->type           = sym->type;
    alias->is_const       = sym->is_const;
    alias->is_public      = sym->is_public;
    alias->source_module  = NULL; // NULL = accessible without qualification
    alias->next           = scope->symbols[index];
    scope->symbols[index] = alias;
}

// Check a use declaration: validate module, look up each symbol, and register unqualified alias
static void check_use_decl(Checker* checker, Node* node) {
    const char* mod_name = node->as.use_decl.module_name;

    // Verify module is imported
    if (!is_imported_module(checker, mod_name)) {
        check_error(checker, node->line, node->column, "Module '%s' is not imported", mod_name);
        return;
    }

    if (node->as.use_decl.is_wildcard) {
        // Wildcard: import all public symbols from the module
        for (Scope* scope = checker->scope; scope; scope = scope->parent) {
            for (int b = 0; b < scope->size; b++) {
                for (Symbol* sym = scope->symbols[b]; sym; sym = sym->next) {
                    if (!sym->source_module || strcmp(sym->source_module, mod_name) != 0) {
                        continue;
                    }
                    if (!sym->is_public) {
                        continue;
                    }
                    // Skip if already defined unqualified (e.g., prelude symbol with same name)
                    if (checker_lookup(checker, sym->name)) {
                        continue;
                    }
                    insert_use_alias(checker, sym, sym->name);
                }
            }
        }
        return;
    }

    for (int i = 0; i < node->as.use_decl.symbol_count; i++) {
        const char* sym_name = node->as.use_decl.symbol_names[i];

        // Look up symbol in the module
        Symbol* sym = checker_lookup_in_module(checker, mod_name, sym_name);
        if (!sym) {
            check_error(checker, node->line, node->column, "No public symbol '%s' in module '%s'",
                        sym_name, mod_name);
            continue;
        }

        // Check for redefinition with a same-module symbol (checker_lookup skips library symbols)
        if (checker_lookup(checker, sym_name)) {
            check_error(checker, node->line, node->column, "'%s' is already defined", sym_name);
            continue;
        }

        insert_use_alias(checker, sym, sym_name);
    }
}

// Type-check a test block: push scope, check body statements, pop scope
static void check_test_decl(Checker* checker, Node* node) {
    checker_push_scope(checker);

    // Check body statements like a void function body
    Node* body = node->as.test_decl.body;
    if (body && body->type == NODE_BLOCK) {
        for (int i = 0; i < body->as.block.stmts.count; i++) {
            check_statement(checker, body->as.block.stmts.nodes[i]);
        }
    }

    checker_pop_scope(checker);
}

// Dispatch declaration type-checking based on node type
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

    case NODE_USE_DECL:
        check_use_decl(checker, node);
        break;

    case NODE_TEST_DECL:
        check_test_decl(checker, node);
        break;

    case NODE_VAR_DECL:
        // Global variable — only const allowed at top level
        if (!node->as.var_decl.is_const) {
            check_error(checker, node->line, node->column,
                        "Top-level variables must use 'const', not 'var'");
            break;
        }
        if (!node->as.var_decl.init) {
            check_error(checker, node->line, node->column,
                        "Top-level 'const' requires an initializer");
            break;
        }
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

typedef enum {
    CHECKER_PASS_DECLARE_TYPES,
    CHECKER_PASS_REGISTER_GENERIC_DECLS,
    CHECKER_PASS_CHECK_LIBRARY_DECLS,
    CHECKER_PASS_CHECK_MAIN_DECLS,
} CheckerPass;

typedef enum {
    CHECKER_MODULE_ALL,
    CHECKER_MODULE_LIBRARY_ONLY,
    CHECKER_MODULE_MAIN_ONLY,
} CheckerModuleFilter;

typedef struct {
    CheckerModuleFilter module_filter;
    int (*decl_filter)(Node* decl);
} CheckerPassSpec;

static int is_main_module(Node* mod) {
    return mod && mod->type == NODE_MODULE && strcmp(mod->as.module.name, "main") == 0;
}

static int is_type_decl(Node* decl) {
    if (!decl) {
        return 0;
    }
    return decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL ||
           decl->type == NODE_TRAIT_DECL;
}

static int is_generic_method_decl(Node* decl) {
    if (!decl || decl->type != NODE_FUNC_DECL) {
        return 0;
    }
    return decl->as.func_decl.receiver_type != NULL &&
           decl->as.func_decl.receiver_type_args.count > 0;
}

static int is_generic_func_decl(Node* decl) {
    if (!decl || decl->type != NODE_FUNC_DECL) {
        return 0;
    }
    return decl->as.func_decl.receiver_type == NULL && decl->as.func_decl.type_param_count > 0;
}

static int is_generic_impl_decl(Node* decl) {
    return decl && decl->type == NODE_IMPL_DECL && decl->as.impl_decl.type_args.count > 0;
}

static int decl_processed_in_early_passes(Node* decl) {
    return is_type_decl(decl) || is_generic_method_decl(decl) || is_generic_impl_decl(decl) ||
           is_generic_func_decl(decl);
}

static int is_generic_registration_decl(Node* decl) {
    return is_generic_method_decl(decl) || is_generic_impl_decl(decl) || is_generic_func_decl(decl);
}

static int is_regular_decl(Node* decl) {
    return !decl_processed_in_early_passes(decl);
}

static int module_matches_filter(Node* mod, CheckerModuleFilter filter) {
    switch (filter) {
    case CHECKER_MODULE_LIBRARY_ONLY:
        return !is_main_module(mod);
    case CHECKER_MODULE_MAIN_ONLY:
        return is_main_module(mod);
    case CHECKER_MODULE_ALL:
        return 1;
    }
    return 0;
}

static void run_checker_pass(Checker* checker, Node* ast, const CheckerPassSpec* pass) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE) {
            continue;
        }
        if (!module_matches_filter(mod, pass->module_filter)) {
            continue;
        }

        checker->modules.current_module = is_main_module(mod) ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (pass->decl_filter(decl)) {
                check_decl(checker, decl);
            }
        }
    }
}

static const CheckerPassSpec k_checker_pass_specs[] = {
    {
        .module_filter = CHECKER_MODULE_ALL,
        .decl_filter   = is_type_decl,
    },
    {
        .module_filter = CHECKER_MODULE_ALL,
        .decl_filter   = is_generic_registration_decl,
    },
    {
        .module_filter = CHECKER_MODULE_LIBRARY_ONLY,
        .decl_filter   = is_regular_decl,
    },
    {
        .module_filter = CHECKER_MODULE_MAIN_ONLY,
        .decl_filter   = is_regular_decl,
    },
};

// Report a missing standalone receiver method for a deferred trait check.
static void report_missing_deferred_trait_method(Checker* checker, DeferredTraitCheck* dc) {
    check_error(checker, dc->line, dc->col, "No standalone method '%s' found for type '%s'",
                dc->method_name, dc->type_name);
}

// Return whether a generic definition contains a body-backed method with the given name.
static int generic_def_has_method_with_body(GenericDef* def, const char* method_name) {
    for (int j = 0; j < def->method_count; j++) {
        Node* m = def->methods[j];
        if (strcmp(m->as.func_decl.name, method_name) == 0 && m->as.func_decl.body != NULL) {
            return 1;
        }
    }
    return 0;
}

// Verify deferred method existence for generic impl targets.
static void verify_generic_deferred_trait_check(Checker* checker, DeferredTraitCheck* dc) {
    GenericDef* def = lookup_generic_def(checker, dc->type_name);
    if (!def || !generic_def_has_method_with_body(def, dc->method_name)) {
        report_missing_deferred_trait_method(checker, dc);
    }
}

// Look up a non-generic standalone method symbol by its receiver-qualified mangled name.
static Symbol* lookup_deferred_standalone_method(Checker* checker, DeferredTraitCheck* dc) {
    char mangled[256];
    snprintf(mangled, sizeof(mangled), "%s_%s", dc->type_name, dc->method_name);
    return checker_lookup(checker, mangled);
}

// Verify non-generic deferred method function signature compatibility against the trait
// expectation.
static int verify_deferred_method_signature(Checker* checker, DeferredTraitCheck* dc,
                                            Type* actual_type) {
    Type* expected = dc->expected_type;

    if (actual_type->kind != TYPE_FUNC || expected->kind != TYPE_FUNC) {
        return 0;
    }

    if (!type_equals(actual_type->as.func.return_type, expected->as.func.return_type)) {
        check_error(checker, dc->line, dc->col,
                    "Method '%s' on '%s' return type mismatch: trait expects '%s', got '%s'",
                    dc->method_name, dc->type_name, type_name(expected->as.func.return_type),
                    type_name(actual_type->as.func.return_type));
        return 0;
    }

    if (actual_type->as.func.param_count != expected->as.func.param_count) {
        check_error(checker, dc->line, dc->col,
                    "Method '%s' on '%s' parameter count mismatch: trait expects %d, got %d",
                    dc->method_name, dc->type_name, expected->as.func.param_count,
                    actual_type->as.func.param_count);
        return 0;
    }

    for (int p = 0; p < expected->as.func.param_count; p++) {
        if (!type_equals(actual_type->as.func.param_types[p], expected->as.func.param_types[p])) {
            check_error(
                checker, dc->line, dc->col,
                "Method '%s' on '%s' parameter %d type mismatch: trait expects '%s', got '%s'",
                dc->method_name, dc->type_name, p + 1, type_name(expected->as.func.param_types[p]),
                type_name(actual_type->as.func.param_types[p]));
        }
    }

    return 1;
}

// Verify receiver mutability for a deferred method against the struct method metadata.
static void verify_deferred_method_constness(Checker* checker, DeferredTraitCheck* dc) {
    Symbol* type_sym = checker_lookup(checker, dc->type_name);
    if (!type_sym || type_sym->kind != SYM_TYPE || type_sym->type->kind != TYPE_STRUCT) {
        return;
    }

    Type* st = type_sym->type;
    for (int j = 0; j < st->as.struc.method_count; j++) {
        if (strcmp(st->as.struc.method_names[j], dc->method_name) == 0) {
            if (st->as.struc.method_is_const[j] != dc->is_const) {
                check_error(checker, dc->line, dc->col,
                            "Method '%s' on '%s' receiver mutability mismatch", dc->method_name,
                            dc->type_name);
            }
            return;
        }
    }
}

// Verify deferred trait checks for non-generic impl targets.
static void verify_non_generic_deferred_trait_check(Checker* checker, DeferredTraitCheck* dc) {
    Symbol* sym = lookup_deferred_standalone_method(checker, dc);
    if (!sym) {
        report_missing_deferred_trait_method(checker, dc);
        return;
    }

    if (!verify_deferred_method_signature(checker, dc, sym->type)) {
        return;
    }

    verify_deferred_method_constness(checker, dc);
}

// Verify deferred trait checks: ensure body-less methods in impl blocks have
// matching standalone receiver methods defined elsewhere.
static void verify_deferred_trait_checks(Checker* checker) {
    for (int i = 0; i < checker->traits.deferred_check_count; i++) {
        DeferredTraitCheck* dc = &checker->traits.deferred_checks[i];

        if (dc->is_generic) {
            verify_generic_deferred_trait_check(checker, dc);
            continue;
        }

        verify_non_generic_deferred_trait_check(checker, dc);
    }
}

// Type-check an entire program AST using four sequential passes.
//
// The multi-pass design is required because declarations can reference each
// other out of order (forward references). The passes are:
//
//   Pass 1: Forward-declare all types (structs, enums, traits) so that
//           fields, function signatures, and type aliases can reference
//           any type regardless of declaration order.
//
//   Pass 2: Register generic methods and trait impls on their GenericDef
//           entries. This must happen before passes 3-4 because type aliases
//           or `new` expressions may trigger generic instantiation, which
//           needs the method templates to be already registered.
//
//   Pass 3: Check all declarations in library modules (non-main). This
//           includes functions, type aliases, impl blocks, and use decls.
//           Library symbols must be fully declared before the main module
//           can reference them.
//
//   Pass 4: Check all declarations in the main module.
//
// Convention: checker->modules.current_module is NULL for the "main" module and
// set to the module name string for library modules. A symbol's
// source_module follows the same convention (NULL = main module).
int checker_check(Checker* checker, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM) {
        return 0;
    }

    checker_push_scope(checker); // Global scope

    // Register builtin Vec<T> as a GenericDef so user methods can be attached
    {
        char** vec_params = xmalloc(sizeof(char*));
        vec_params[0]     = xstrdup("T");
        register_generic_def(checker, "Vec", vec_params, NULL, 1, NULL);
        free(vec_params[0]);
        free(vec_params);
    }

    // Register builtin Box<T> as a GenericDef (prelude so user structs can shadow it)
    {
        const char* saved_module        = checker->modules.current_module;
        checker->modules.current_module = "prelude";
        char** box_params               = xmalloc(sizeof(char*));
        box_params[0]                   = xstrdup("T");
        register_generic_def(checker, "Box", box_params, NULL, 1, NULL);
        free(box_params[0]);
        free(box_params);
        checker->modules.current_module = saved_module;
    }

    for (size_t i = 0; i < sizeof(k_checker_pass_specs) / sizeof(k_checker_pass_specs[0]); i++) {
        run_checker_pass(checker, ast, &k_checker_pass_specs[i]);
    }

    verify_deferred_trait_checks(checker);

    checker->modules.current_module = NULL;
    checker_pop_scope(checker);

    return checker->error_count == 0;
}
