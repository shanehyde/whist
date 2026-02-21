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

// RC-managed variable tracking entry
typedef struct {
    char* name;
    Type* type; // Type* for the RC variable (not owned)
    int   scope_depth;
} RcVar;

// Name alias: maps a Whist-side name to a C-side name
typedef struct {
    char* whist_name;
    char* c_name;
} NameAlias;

// --- CodeGen sub-structures ---

typedef struct {
    FILE* file;
    int   indent;
    int   temp_count; // For generating temporary variables
} CodeGenOutput;

typedef struct {
    Node** stack;       // Stack of deferred statements
    int    count;       // Number of deferred statements
    int    capacity;    // Capacity of defer stack
    Node*  return_type; // Return type of current function (for __ret variable)
} CodeGenDefer;

typedef struct {
    RcVar* vars;
    int    count;
    int    capacity;
    int    depth;
    int    debug;
} CodeGenRc;

typedef struct {
    TypeSubstContext* subst;
    Node*             tmpl;    // Current generic struct template
    char**            modules; // Accessible modules for current generic method (not owned)
    int               module_count;
} CodeGenGenerics;

typedef struct {
    GenericInstance*     instances;
    int                  instance_count;
    SpanInstance*        spans;
    int                  span_count;
    VecInstance*         vecs;
    int                  vec_count;
    BoxInstance*         boxes;
    int                  box_count;
    TraitImpl*           traits;
    int                  trait_count;
    GenericFuncDef*      func_defs;
    int                  func_def_count;
    GenericFuncInstance* func_instances;
    int                  func_instance_count;
    SemInfo*             sem;
} CodeGenChecker;
// CodeGenChecker pointers are borrowed from checker state (not owned by codegen).

typedef struct {
    char** names;
    int    count;
    int    capacity;
    int*   has_rc_fields; // Parallel to names
    int*   has_eq;        // Parallel to names: 1 if data enum supports ==
} CodeGenEnums;

typedef struct {
    // Type alias tracking (alias name -> target type node)
    char** types;
    Node** type_targets;
    int    type_count;
    int    type_capacity;
    // Extern function alias tracking (Whist name -> C name)
    NameAlias* externs;
    int        extern_count;
    int        extern_capacity;
    // Extern function name tracking (all extern functions)
    char** extern_funcs;
    int    extern_func_count;
    int    extern_func_capacity;
    // Use declaration aliases (unqualified Whist name -> qualified C name)
    NameAlias* uses;
    int        use_count;
    int        use_capacity;
} CodeGenAliases;

typedef struct {
    CodeGenOutput   out;
    CodeGenDefer    defer;
    CodeGenRc       rc;
    CodeGenGenerics generics;
    CodeGenChecker  checker;
    CodeGenEnums    enums;
    CodeGenAliases  aliases;
    // Tuple typedef tracking
    Type** tuple_types;
    int    tuple_type_count;
    int    tuple_type_capacity;
    // RC string literal table (immortal static structs)
    struct {
        char** values;  // String values (owned copies)
        int*   lengths; // String lengths
        int    count;
        int    capacity;
    } string_lits;
    // Hoisted anonymous new expressions (for RC cleanup after calls)
    struct {
        Node** nodes; // new expr node pointers (not owned)
        char** names; // corresponding hoisted temp names (owned)
        int    count;
        int    capacity;
    } hoist;
    // Lambda functions collected for top-level emission
    struct {
        Node** nodes; // NODE_LAMBDA AST nodes (not owned)
        int    count;
        int    capacity;
    } lambdas;
    // Thunks for named functions used as closure values
    struct {
        char** c_names;    // C function names (owned copies)
        Type** func_types; // TYPE_FUNC for each (not owned)
        int    count;
        int    capacity;
    } thunks;
    // Capture context: names of captured variables in current lambda body
    // When set, emit_ident checks this and emits __cenv->name for captures
    struct {
        char** names;
        int    count;
    } capture_ctx;
    // Current module name (NULL for "main")
    const char* current_module;
    // 1 if currently emitting an enum method body
    int in_enum_method;
    // Test mode: 1 = emit test runner instead of user main
    int test_mode;
    int test_index; // Counter for emitting __test_N function names
    // Source file path for assert error messages
    const char* source_file;
    // 1 = emit #line directives in generated C
    int line_directives;
    // Non-NULL in -c mode: only emit code for this module (skip imported modules)
    const char* target_module;
    // 1 when emitting non-target module declarations (forces static linkage)
    int force_static;
} CodeGen;

// Codegen lifecycle:
// - codegen_init stores borrowed checker-derived metadata and output handles.
// - codegen_emit consumes AST + semantic annotations to emit C; it does not own AST/checker data.
void codegen_init(CodeGen* gen, FILE* out, CodeGenChecker checker_data, int rc_debug, int test_mode,
                  const char* source_file, int line_directives);
void codegen_emit(CodeGen* gen, Node* ast);
void codegen_free(CodeGen* gen);

#endif
