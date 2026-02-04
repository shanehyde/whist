#ifndef WHIST_CODEGEN_H
#define WHIST_CODEGEN_H

#include <stdio.h>

#include "ast.h"
#include "checker.h"
#include "types.h"

// Type parameter substitution context for generic method emission
typedef struct {
    char** type_params; // Type parameter names ["T", "K", ...]
    Type** type_args;   // Concrete types to substitute
    int    count;
} TypeSubstContext;

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
    // Type substitution for generic methods
    TypeSubstContext* subst_ctx;
    // Tuple typedef tracking
    Type** tuple_types; // Array of unique tuple types
    int    tuple_type_count;
    int    tuple_type_capacity;
    // Generic instances from checker (not owned, do not free)
    GenericInstance* generic_instances;
    int              generic_instance_count;
    // Span instances from checker (not owned, do not free)
    SpanInstance* span_instances;
    int           span_instance_count;
} CodeGen;

void codegen_init(CodeGen* gen, FILE* out, GenericInstance* generic_instances, int generic_count,
                  SpanInstance* span_instances, int span_count);
void codegen_emit(CodeGen* gen, Node* ast);

#endif
