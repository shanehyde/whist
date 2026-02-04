#ifndef WHIST_CODEGEN_H
#define WHIST_CODEGEN_H

#include <stdio.h>

#include "ast.h"
#include "checker.h"
#include "types.h"

// Generic struct instantiation info for codegen
typedef struct {
    char*  mangled_name; // "Box_i64"
    char*  base_name;    // "Box"
    Type** type_args;    // [i64]
    int    type_arg_count;
    Type*  struct_type; // The instantiated TYPE_STRUCT
} GenericCodegenInfo;

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
    // Tuple typedef tracking
    Type** tuple_types; // Array of unique tuple types
    int    tuple_type_count;
    int    tuple_type_capacity;
    // Generic type tracking
    GenericCodegenInfo* generic_instances;
    int                 generic_instance_count;
    int                 generic_instance_capacity;
} CodeGen;

void codegen_init(CodeGen* gen, FILE* out);
void codegen_emit(CodeGen* gen, Node* ast);

#endif
