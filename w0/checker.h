#ifndef WHIST_CHECKER_H
#define WHIST_CHECKER_H

#include "ast.h"
#include "types.h"

typedef struct Symbol Symbol;
typedef struct Scope Scope;
typedef struct Checker Checker;

typedef enum {
    SYM_VAR,
    SYM_FUNC,
    SYM_TYPE,
    SYM_ENUM_VALUE,
} SymbolKind;

struct Symbol {
    SymbolKind kind;
    char *name;
    Type *type;
    int is_const;
    Symbol *next;  // Hash chain
};

struct Scope {
    Symbol **symbols;  // Hash table
    int size;
    Scope *parent;
};

struct Checker {
    Scope *scope;
    Type *current_func_return;  // Return type of current function
    int in_loop;                // Are we inside a loop?
    int error_count;
    char error_msg[256];
};

void checker_init(Checker *checker);
void checker_free(Checker *checker);

int checker_check(Checker *checker, Node *ast);

// Scope operations
void checker_push_scope(Checker *checker);
void checker_pop_scope(Checker *checker);
Symbol *checker_define(Checker *checker, const char *name, SymbolKind kind, Type *type);
Symbol *checker_lookup(Checker *checker, const char *name);
Symbol *checker_lookup_local(Checker *checker, const char *name);

#endif
