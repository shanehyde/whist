#include "checker_util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void check_error(Checker* checker, int line, int col, const char* fmt, ...) {
    checker->error_count++;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[line %d:%d] Error: ", line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int          c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void checker_init(Checker* checker) {
    checker->scope               = NULL;
    checker->current_func_return = NULL;
    checker->in_loop             = 0;
    checker->error_count         = 0;
    checker->error_msg[0]        = '\0';
    types_init();
}

void checker_free(Checker* checker) {
    while (checker->scope) {
        checker_pop_scope(checker);
    }
    types_cleanup();
}

void checker_push_scope(Checker* checker) {
    Scope* scope = calloc(1, sizeof(Scope));
    if (!scope) {
        fprintf(stderr, "Out of memory\n");
        return;
    }
    scope->symbols = calloc(SCOPE_SIZE, sizeof(Symbol*));
    if (!scope->symbols) {
        fprintf(stderr, "Out of memory\n");
        free(scope);
        return;
    }
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
            free(sym);
            sym = next;
        }
    }
    free(scope->symbols);
    free(scope);
}

Type* resolve_type(Checker* checker, Node* type_node) {
    if (!type_node)
        return type_void;

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;
        if (strcmp(name, "void") == 0)
            return type_void;
        if (strcmp(name, "bool") == 0)
            return type_bool;
        if (strcmp(name, "i64") == 0)
            return type_int64;
        if (strcmp(name, "i8") == 0)
            return type_int8;
        if (strcmp(name, "i16") == 0)
            return type_int16;
        if (strcmp(name, "i32") == 0)
            return type_int32;
        if (strcmp(name, "u64") == 0)
            return type_uint64;
        if (strcmp(name, "u8") == 0)
            return type_uint8;
        if (strcmp(name, "u16") == 0)
            return type_uint16;
        if (strcmp(name, "u32") == 0)
            return type_uint32;
        if (strcmp(name, "f32") == 0)
            return type_f32;
        if (strcmp(name, "f64") == 0)
            return type_f64;
        if (strcmp(name, "char") == 0)
            return type_char;
        if (strcmp(name, "string") == 0)
            return type_string;

        // Look up user-defined type
        Symbol* sym = checker_lookup(checker, name);
        if (sym && sym->kind == SYM_TYPE) {
            return sym->type;
        }
        check_error(checker, type_node->line, type_node->column, "Unknown type '%s'", name);
        return type_error;
    }
    case NODE_UNARY:
        // Pointer types no longer supported
        check_error(checker, type_node->line, type_node->column,
                    "Pointer types are no longer supported");
        return type_error;
    case NODE_INDEX:
        // Array type: [n]T
        {
            Type* elem = resolve_type(checker, type_node->as.index.object);
            int   size = -1;
            if (type_node->as.index.index) {
                // For now, only support constant integer sizes
                if (type_node->as.index.index->type == NODE_INT_LIT) {
                    size = (int)type_node->as.index.index->as.int_lit.value;
                }
            }
            return type_array(elem, size);
        }
    default:
        break;
    }

    check_error(checker, type_node->line, type_node->column, "Invalid type");
    return type_error;
}
