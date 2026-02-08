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
    int        is_rc;         // 1 if variable is RC-managed (allocated with new)
    char*      source_module; // NULL = same module, else external module name
    Symbol*    next;          // Hash chain
};

struct Scope {
    Symbol** symbols; // Hash table
    int      size;
    Scope*   parent;
};

// Trait implementation record
typedef struct {
    char* trait_name;
    char* type_name;
} TraitImpl;

// Method registered on a primitive type (from trait impls)
typedef struct {
    char* type_name;   // "i32", "u32", etc.
    char* method_name; // "hash"
    Type* method_type; // function type (params + return)
    int   is_const;    // const receiver?
} PrimitiveMethod;

// Generic struct definition (template)
typedef struct {
    char*  name;              // "Box", "Pair"
    char** type_params;       // ["T"] or ["K", "V"]
    char** type_param_bounds; // Trait bounds (NULL entries = unbounded)
    int    type_param_count;
    Node*  decl;          // Original AST node for field type resolution
    int    is_type_alias; // 1 if this is a generic type alias, 0 for struct/enum
    // Methods on the generic struct
    Node** methods; // Array of NODE_FUNC_DECL
    int    method_count;
    int    method_capacity;
} GenericDef;

// Instantiated generic struct
typedef struct {
    char*  mangled_name; // "Box_i64", "Pair_i64_string"
    char*  base_name;    // "Box", "Pair" (for finding template)
    Type*  type;         // Concrete TYPE_STRUCT
    Type** type_args;    // Resolved type arguments [i64], [i64, string]
    int    type_arg_count;
} GenericInstance;

// Instantiated span type
typedef struct {
    char* mangled_name; // "Span_i64"
    Type* elem_type;    // The element type
    Type* type;         // The TYPE_SPAN instance
} SpanInstance;

// Instantiated vec type
typedef struct {
    char* mangled_name; // "Vec_i64"
    Type* elem_type;    // The element type
    Type* type;         // The TYPE_VEC instance
} VecInstance;

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

    // Current module being processed (NULL = main module)
    const char* current_module;

    // Generic struct definitions (templates)
    GenericDef* generic_defs;
    int         generic_def_count;
    int         generic_def_capacity;

    // Instantiated generic structs
    GenericInstance* generic_instances;
    int              generic_instance_count;
    int              generic_instance_capacity;

    // Context for type parameter substitution during instantiation
    char** current_type_params; // Type parameter names
    Type** current_type_args;   // Concrete types for each param
    int    current_type_param_count;

    // Instantiated span types
    SpanInstance* span_instances;
    int           span_instance_count;
    int           span_instance_capacity;

    // Instantiated vec types
    VecInstance* vec_instances;
    int          vec_instance_count;
    int          vec_instance_capacity;

    // Trait implementations
    TraitImpl* trait_impls;
    int        trait_impl_count;
    int        trait_impl_capacity;

    // Methods on primitive types (from trait impls)
    PrimitiveMethod* primitive_methods;
    int              primitive_method_count;
    int              primitive_method_capacity;

    // Type alias cycle detection
    int alias_depth;

    // Hint for generic enum type inference (set by var_decl when declared type is known)
    Type* enum_target_hint;
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

// Module-qualified access
int     is_imported_module(Checker* checker, const char* name);
Symbol* checker_lookup_in_module(Checker* checker, const char* module_name,
                                 const char* symbol_name);

#endif
