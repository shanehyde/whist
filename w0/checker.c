#include "checker.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check_decl.h"
#include "checker_util.h"

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

    Symbol* sym           = calloc(1, sizeof(Symbol));
    sym->kind             = kind;
    sym->name             = strdup(name);
    sym->type             = type;
    sym->is_const         = is_const;
    sym->is_public        = is_public;
    sym->source_module    = source_module ? strdup(source_module) : NULL;
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

Symbol* checker_lookup(Checker* checker, const char* name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                // Check module visibility
                if (!is_module_accessible(checker, sym->source_module)) {
                    continue; // Symbol exists but not accessible from this module
                }
                // Symbols from library imports must be public to be accessible
                // Exception: private symbols are accessible if we're in the same module
                int same_module = (sym->source_module == NULL && checker->current_module == NULL) ||
                                  (sym->source_module && checker->current_module &&
                                   strcmp(sym->source_module, checker->current_module) == 0);
                if (sym->source_module && !sym->is_public && !same_module) {
                    continue; // Private symbol from library import
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

int checker_check(Checker* checker, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM) {
        return 0;
    }

    checker_push_scope(checker); // Global scope

    // First pass: declare all types (structs, enums) for forward references
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Set current module context (NULL for "main", module name for library imports)
        checker->current_module =
            strcmp(mod->as.module.name, "main") == 0 ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    // Second pass: check everything else
    // Process library modules (non-main) first, then main module last
    // This ensures library functions are declared before main uses them
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Skip main module in this pass
        if (strcmp(mod->as.module.name, "main") == 0)
            continue;
        checker->current_module = mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL && decl->type != NODE_ENUM_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    // Third pass: check main module
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        if (strcmp(mod->as.module.name, "main") != 0)
            continue;
        checker->current_module = NULL;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL && decl->type != NODE_ENUM_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    checker->current_module = NULL;
    checker_pop_scope(checker);

    return checker->error_count == 0;
}
