#include "checker.h"

#include <stdlib.h>
#include <string.h>

#include "check_decl.h"
#include "checker_util.h"

Symbol* checker_define(Checker* checker, const char* name, SymbolKind kind, Type* type,
                       int is_const, int is_public) {
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
    sym->next             = scope->symbols[index];
    scope->symbols[index] = sym;
    return sym;
}

Symbol* checker_lookup(Checker* checker, const char* name) {
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

    // First pass: declare all types and functions (for forward references)
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        Node* decl = ast->as.program.decls.nodes[i];
        if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL) {
            check_decl(checker, decl);
        }
    }

    // Second pass: check everything
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        Node* decl = ast->as.program.decls.nodes[i];
        if (decl->type != NODE_STRUCT_DECL && decl->type != NODE_ENUM_DECL) {
            check_decl(checker, decl);
        }
    }

    checker_pop_scope(checker);

    return checker->error_count == 0;
}
