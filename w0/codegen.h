#ifndef WHIST_CODEGEN_H
#define WHIST_CODEGEN_H

#include <stdio.h>

#include "ast.h"
#include "checker.h"
#include "types.h"

typedef struct {
    FILE* out;
    int   indent;
    int   temp_count; // For generating temporary variables
    // Defer support
    Node**      defer_stack;         // Stack of deferred statements
    int         defer_count;         // Number of deferred statements
    int         defer_capacity;      // Capacity of defer stack
    Node*       current_return_type; // Return type of current function (for __ret variable)
    const char* current_module;      // Current module name (NULL for "main")
} CodeGen;

void codegen_init(CodeGen* gen, FILE* out);
void codegen_emit(CodeGen* gen, Node* ast);

#endif
