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
    // RC variable tracking
    struct {
        char* name;
        char* dec_func; // "__rc_dec" or "__rc_dec_TypeName"
        Type* type;     // Type* for the RC variable (not owned)
        int   scope_depth;
    }*  rc_vars;
    int rc_var_count;
    int rc_var_capacity;
    int rc_scope_depth;
    int rc_debug;
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
    // Trait implementations from checker (not owned, do not free)
    TraitImpl* trait_impls;
    int        trait_impl_count;
    // Enum name tracking (for distinguishing enums from structs in type emission)
    char** enum_names;
    int    enum_name_count;
    int    enum_name_capacity;
    int*   enum_has_rc_fields; // Parallel to enum_names
    // Type alias tracking (alias name -> target type node)
    char** alias_names;
    Node** alias_targets;
    int    alias_count;
    int    alias_capacity;
} CodeGen;

void codegen_init(CodeGen* gen, FILE* out, GenericInstance* generic_instances, int generic_count,
                  SpanInstance* span_instances, int span_count, TraitImpl* trait_impls,
                  int trait_impl_count, int rc_debug);
void codegen_emit(CodeGen* gen, Node* ast);

#endif
