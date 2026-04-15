#ifndef WHIST_CHECKER_H
#define WHIST_CHECKER_H

#include "ast.h"
#include "types.h"

typedef struct Symbol  Symbol;
typedef struct Scope   Scope;
typedef struct Checker Checker;
typedef struct SemInfo SemInfo;

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
    int      is_lambda_boundary; // 1 = marks lambda scope boundary
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
    char*       name;              // "Box", "Pair"
    char**      type_params;       // ["T"] or ["K", "V"]
    char**      type_param_bounds; // Trait bounds (NULL entries = unbounded)
    int         type_param_count;
    Node*       decl;          // Original AST node for field type resolution
    int         is_type_alias; // 1 if this is a generic type alias, 0 for struct/enum
    const char* source_module; // "prelude" or NULL — for shadowing support
    // Methods on the generic struct
    Node** methods; // Array of NODE_FUNC_DECL
    int    method_count;
    int    method_capacity;
    int    has_init; // 1 if an inherent impl block defines an init method
} GenericDef;

// Instantiated generic struct
typedef struct {
    char*  mangled_name; // "Box_i64", "Pair_i64_string"
    char*  base_name;    // "Box", "Pair" (for finding template)
    Type*  type;         // Concrete TYPE_STRUCT
    Type** type_args;    // Resolved type arguments [i64], [i64, string]
    int    type_arg_count;
    // Per-instantiation cloned method bodies (parallel to GenericDef.methods)
    Node** method_bodies; // Array of cloned body nodes (NULL entries for skipped methods)
    int    method_body_count;
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
    // User-defined methods (instantiated per Vec<T>)
    char** method_names;
    Type** method_types;
    int*   method_is_const;
    int    method_count;
    int    method_capacity;
    // Cloned method bodies for codegen (parallel to GenericDef.methods ordering)
    Node** method_bodies;
    int    method_body_count;
    int    methods_instantiated; // Whether user methods have been populated
} VecInstance;

// Instantiated box type
typedef struct {
    char* mangled_name; // "Box_i64"
    Type* elem_type;    // The element type
    Type* type;         // The TYPE_BOX instance
} BoxInstance;

// Generic free function definition (template)
// Also used for method-level generics: func (Vec<T>) map<K>(...) -> Vec<K>
// For methods: type_params = combined receiver-bound + method params ["T", "K"].
// receiver_param_count stores how many leading entries come from receiver pattern
// bindings (not the receiver generic arity).
typedef struct {
    char*       name;
    char**      type_params;       // ["T"] or ["T", "U"]
    char**      type_param_bounds; // Trait bounds parallel to type_params (NULL = unbounded)
    int         type_param_count;
    Node*       decl; // Original AST node (NODE_FUNC_DECL)
    const char* source_module;
    // Method-level generics (NULL/0 for free functions)
    char* receiver_type;        // "Vec" for methods, NULL for free funcs
    int   receiver_param_count; // Count of receiver-bound vars (e.g., 1 for Pair<i32, Box<T>>)
} GenericFuncDef;

// Instantiated generic free function (also used for method-level generics)
typedef struct {
    char*  mangled_name; // "identity_i64" or "Vec_i64_map_string"
    char*  base_name;    // "identity" or "Vec.map"
    Type*  func_type;    // Concrete TYPE_FUNC
    Type** type_args;
    int    type_arg_count;
    Node*  body; // Cloned + type-checked body
    // Method-level generic fields (0/NULL for free functions)
    int   is_method;         // 1 if method instance, 0 for free func
    char* receiver_type;     // "Vec" for methods, NULL for free funcs
    Type* receiver_concrete; // Concrete receiver type (e.g., TYPE_VEC with elem=i64)
} GenericFuncInstance;

// Module import tracking
typedef struct {
    char**      direct_imports;
    int         direct_imports_count;
    char**      current_accessible_modules;
    int         current_accessible_modules_count;
    const char* current_module; // NULL = main module
} CheckerModules;

// Generic definitions and instantiations
typedef struct {
    GenericDef*      defs;
    int              def_count;
    int              def_capacity;
    GenericInstance* instances;
    int              instance_count;
    int              instance_capacity;
    char**           current_type_params; // Type parameter names
    Type**           current_type_args;   // Concrete types for each param
    int              current_type_param_count;
    // Generic free functions
    GenericFuncDef*      func_defs;
    int                  func_def_count;
    int                  func_def_capacity;
    GenericFuncInstance* func_instances;
    int                  func_instance_count;
    int                  func_instance_capacity;
} CheckerGenerics;

// Instantiated container types (Span, Vec, Box)
typedef struct {
    SpanInstance* spans;
    int           span_count;
    int           span_capacity;
    VecInstance*  vecs;
    int           vec_count;
    int           vec_capacity;
    BoxInstance*  boxes;
    int           box_count;
    int           box_capacity;
} CheckerContainers;

// Deferred trait check: body-less method in impl block (duck-type conformance)
typedef struct {
    char* type_name;
    char* method_name;
    Type* expected_type; // Function type from trait (Self substituted)
    int   is_const;
    int   is_generic;
    int   line;
    int   col;
} DeferredTraitCheck;

// Trait implementations and primitive methods
typedef struct {
    TraitImpl*       impls;
    int              impl_count;
    int              impl_capacity;
    PrimitiveMethod* primitive_methods;
    int              primitive_method_count;
    int              primitive_method_capacity;
    // Deferred trait checks (body-less methods in impl blocks)
    DeferredTraitCheck* deferred_checks;
    int                 deferred_check_count;
    int                 deferred_check_capacity;
} CheckerTraits;

struct Checker {
    Scope* scope;
    Type*  current_func_return; // Return type of current function
    int    in_loop;             // Are we inside a loop?
    int    in_init;             // Are we inside an init() method?
    int    error_count;

    CheckerModules    modules;
    CheckerGenerics   generics;
    CheckerContainers containers;
    CheckerTraits     traits;

    // Type alias cycle detection
    int alias_depth;

    // Forward reference support: when set, type decl functions only register
    // the type name (no field/variant resolution). Cleared before the full pass.
    int registering_type_names;

    // Hint for generic enum type inference (set by var_decl when declared type is known)
    Type* enum_target_hint;

    // Expected function type for lambda parameter inference (set before checking lambda args)
    Type* expected_func_type;

    // Self type: set to TYPE_GENERIC_PARAM("Self") in trait decls,
    // concrete implementing type in impl blocks; NULL otherwise.
    Type* self_type;

    // Private field access: base type name of receiver when inside a method
    const char* current_method_receiver;

    // Lambda tracking
    int    lambda_next_id; // Next lambda ID (monotonically increasing)
    int    lambda_depth;   // >0 when inside a lambda body
    Node** lambda_stack;   // Stack of enclosing lambda nodes (innermost last)
    int    lambda_stack_count;
    int    lambda_stack_capacity;

    // Sidecar semantic metadata used by checker/codegen; owned by checker.
    SemInfo* sem;
};

// Checker lifecycle:
// - checker_init/checker_free own checker-internal allocations and Type arena cleanup.
// - checker_check performs semantic analysis over an existing AST and may attach semantic
//   annotations used by codegen.
// - AST syntax identity (token spelling/shape) should be treated as parser-owned.
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
