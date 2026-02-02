#ifndef WHIST_CHECKER_H
#define WHIST_CHECKER_H

#include "ast.h"
#include "types.h"

typedef struct Symbol  Symbol;
typedef struct Scope   Scope;
typedef struct Checker Checker;

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE,
    SYM_ENUM_VALUE,
} SymbolKind;

struct Symbol {
    SymbolKind kind;
    char*      name;
    Type*      type;
    int        is_const;
    int        is_public;
    char*      source_module; // NULL = same module, else external module name
    Symbol*    next;          // Hash chain
};

struct Scope {
    Symbol** symbols; // Hash table
    int      size;
    Scope*   parent;
};

struct Checker {
    Scope* scope;
    Type*  current_func_return; // Return type of current function
    int    in_loop;             // Are we inside a loop?
    int    error_count;
    char   error_msg[256];

    // Library modules directly imported by the root file (for global scope)
    char** direct_imports;
    int    direct_imports_count;

    // Library modules accessible from the current function (NULL = use direct_imports)
    char** current_accessible_modules;
    int    current_accessible_modules_count;
};

void checker_init(Checker* checker);
void checker_set_direct_imports(Checker* checker, char** direct_imports, int count);
void checker_free(Checker* checker);

int checker_check(Checker* checker, Node* ast);

// Scope operations
void    checker_push_scope(Checker* checker);
void    checker_pop_scope(Checker* checker);
Symbol* checker_define(Checker* checker, const char* name, SymbolKind kind, Type* type,
                       int is_const, int is_public, const char* source_module);
Symbol* checker_lookup(Checker* checker, const char* name);
Symbol* checker_lookup_any(Checker* checker, const char* name); // Ignores module visibility
Symbol* checker_lookup_local(Checker* checker, const char* name);

#endif
