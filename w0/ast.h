#ifndef WHIST_AST_H
#define WHIST_AST_H

#include "lexer.h"

// Forward declarations
typedef struct DestructPattern DestructPattern;
typedef struct Type            Type;

// Destructuring pattern kinds
typedef enum {
    PATTERN_IDENT, // Single identifier: x
    PATTERN_TUPLE, // Nested tuple: (a, b) or (a, (b, c))
    PATTERN_STRUCT // Struct fields: {x, y}
} PatternKind;

// Recursive destructuring pattern for tuple unpacking
struct DestructPattern {
    PatternKind kind;
    union {
        struct {
            char* name;
            int   name_length;
        } ident;
        struct {
            DestructPattern** elements;
            int               count;
        } tuple;
        struct {
            char** field_names;
            int*   field_name_lengths;
            char** local_names; // Rename targets (same as field_names if no rename)
            int*   local_name_lengths;
            int    count;
            int    capacity;
            Type** field_types; // Set by checker (parallel array)
        } struc;
    } as;
    Type* resolved_type; // Set by checker
};

// Pattern memory management
DestructPattern* pattern_new_ident(const char* name, int length);
DestructPattern* pattern_new_tuple(int capacity);
void             pattern_tuple_push(DestructPattern* pattern, DestructPattern* elem);
DestructPattern* pattern_new_struct(int capacity);
void             pattern_struct_push(DestructPattern* pattern, const char* name, int length,
                                     const char* local_name, int local_length);
void             pattern_free(DestructPattern* pattern);
DestructPattern* pattern_clone(DestructPattern* pattern);

typedef enum {
    // Expressions
    NODE_INT_LIT,
    NODE_FLOAT_LIT,
    NODE_STRING_LIT,
    NODE_CHAR_LIT,
    NODE_BOOL_LIT,
    NODE_NULL_LIT,
    NODE_IDENT,
    NODE_BINARY,
    NODE_UNARY,
    NODE_CALL,
    NODE_INDEX,
    NODE_SLICE,
    NODE_MEMBER,
    NODE_ASSIGN,
    NODE_STRUCT_INIT,
    NODE_FIELD_INIT,
    NODE_ENUM_VALUE,
    NODE_NEW_EXPR,
    NODE_STRING_INTERP,
    NODE_CAST,
    NODE_TRY_EXPR,
    NODE_LAMBDA,

    // Statements
    NODE_EXPR_STMT,
    NODE_VAR_DECL,
    NODE_BLOCK,
    NODE_IF,
    NODE_IF_LET,
    NODE_WHILE,
    NODE_FOR,
    NODE_FOREACH,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_DEFER,
    NODE_MATCH,
    NODE_MATCH_ARM,

    // Declarations
    NODE_FUNC_DECL,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,
    NODE_ENUM_VARIANT,
    NODE_TRAIT_DECL,
    NODE_IMPL_DECL,
    NODE_TYPE_ALIAS,
    NODE_USE_DECL,
    NODE_TEST_DECL,

    NODE_EXTERN_MODULE,
    NODE_MODULE,

    // Tuple types
    NODE_TUPLE_TYPE, // (T1, T2, ...)
    NODE_TUPLE_LIT,  // (e1, e2, ...)

    // Array literal
    NODE_ARRAY_LIT, // [e1, e2, ...]

    // Array type: [n]T or []T
    NODE_ARRAY_TYPE,

    // Generic types
    NODE_GENERIC_TYPE, // Box<i64>, Pair<K, V>

    // Function type
    NODE_FUNC_TYPE, // func(T1, T2) -> ReturnType

    // Other
    NODE_PARAM,
    NODE_FIELD,
    NODE_PROGRAM
} NodeType;

typedef struct Node     Node;
typedef struct NodeList NodeList;

struct NodeList {
    Node** nodes;
    int    count;
    int    capacity;
};

typedef struct {
    int   is_public;
    int   is_extern;
    char* receiver_type;     // Method receiver struct name (NULL for regular functions)
    int   receiver_type_len; // Length of receiver type name
    int   receiver_is_const; // 1 if const receiver, 0 if mutable
    // For generic method receivers: func (Box<T>) get() -> T
    // or func (Pair<i32, Box<T>>) set() -> void
    NodeList receiver_type_args; // Type nodes for type arguments in receiver
    char*    name;
    int      name_length;
    // Generic type parameters for free functions: func identity<T>(x: T) -> T
    char**   type_params;       // ["T"] or ["T", "U"]
    char**   type_param_bounds; // Trait bounds parallel to type_params (NULL entries = unbounded)
    int      type_param_count;
    char*    extern_name; // Original C function name (when 'as' alias used), NULL otherwise
    int      extern_name_length;
    NodeList params;
    int      is_varargs;
    int      return_is_const; // 1 if return type is const-qualified, 0 otherwise
    Node*    return_type;
    Node*    body;
    char*    source_module; // NULL = same module, else external module name

    // Library modules accessible from this function's body
    char** accessible_modules;
    int    accessible_modules_count;
} func_decl_node;

typedef struct {
    int   is_public;
    char* name;        // NULL if destructuring
    int   name_length; // 0 if destructuring
    Node* type;
    Node* init;
    int   is_const;
    int   is_boxed;      // Parser flag: var ^name = expr (autoboxing)
    char* source_module; // NULL = same module, else external module name

    // Destructuring support: var (a, b) = tuple; or var (a, (b, c)) = nested;
    DestructPattern* destruct_pattern; // NULL if not destructuring
    int              is_rc; // Set by checker: 1 if init is a new expression or copy of RC var
    Type*            resolved_type; // Set by checker for RC vars (the struct type)
} var_decl_node;

struct Node {
    NodeType type;
    int      line;
    int      column;
    int      is_owned_temp;   // Set by checker: expression produces an owned RC value
    Type*    owned_temp_type; // Set by checker: the RC type of the owned temporary
    int      is_box_deref;    // Set by checker: auto-deref Box<T> to T (emit ->value)

    union {
        // Literals
        struct {
            long value;
        } int_lit;
        struct {
            double value;
        } float_lit;
        struct {
            char* value;
            int   length;
        } string_lit;
        struct {
            char value;
        } char_lit;
        struct {
            int value;
        } bool_lit;

        // Identifier
        struct {
            char* name;
            int   length;
            Type* resolved_func_type; // Set by checker: TYPE_FUNC when ident refers to SYM_FUNC
        } ident;

        // Binary expression
        struct {
            TokenType op;
            Node*     left;
            Node*     right;
            int       is_string_op; // Set by checker for string ==, !=, <, >, <=, >=, +
            int       is_eq_op;     // Set by checker for struct/data-enum == !=
            int       is_enum_eq;   // Set by checker: 1 if eq is for a data enum
            char*     eq_type_name; // Set by checker: type name for Eq method call
        } binary;

        // Unary expression
        struct {
            TokenType op;
            Node*     operand;
        } unary;

        // Function call
        struct {
            Node*    func;
            NodeList args;
            char*    resolved_name;    // Set by checker: mangled name for generic function calls
            int      is_indirect_call; // Set by checker: 1 if call through closure (SYM_VAR callee)
            Type*    resolved_func_type; // Set by checker: callee TYPE_FUNC for indirect calls
        } call;

        // Index expression: arr[index]
        struct {
            Node* object;
            Node* index;
            int   is_tuple_index; // Set by checker if indexing a tuple
            int   is_span_index;  // Set by checker if indexing a span
            int   is_vec_index;   // Set by checker if indexing a vec
            int   is_rc_elem;     // Set by checker if vec element type is RC-managed
        } index;

        // Slice expression: arr[start:end]
        struct {
            Node* object;        // Array or span being sliced
            Node* start;         // Start index (NULL if omitted)
            Node* end;           // End index (NULL if omitted)
            Type* resolved_type; // Set by checker
            int   is_array;      // Set by checker: 1 if slicing array, 0 if slicing span
            int   is_vec;        // Set by checker: 1 if slicing a vec
            int   is_string;     // Set by checker: 1 if slicing a string
        } slice;

        // Member access: obj.member
        struct {
            Node* object;
            char* name;
            int   length;
            int   is_ref;             // Set by checker: 1 if object is a struct reference
            int   is_const_access;    // Set by checker: 1 if accessing a const field
            char* struct_name;        // Set by checker if this is a method access (NULL otherwise)
            char* module_name;        // Set by checker for module-qualified access (e.g., "std")
            int   is_method_ref;      // Set by checker: 1 if Type.method unbound reference
            Type* resolved_func_type; // Set by checker: TYPE_FUNC for method refs
        } member;

        // Assignment
        struct {
            TokenType op; // TOK_EQ, TOK_PLUS_EQ, etc.
            Node*     target;
            Node*     value;
        } assign;

        // Struct initializer
        struct {
            NodeList fields;
        } struct_init;

        // Field initializer
        struct {
            char* name;
            int   name_length;
            Node* value;
        } field_init;

        // Enum value access: EnumName::ValueName or EnumName::ValueName(args)
        // Also used for module access: module::func or module::func(args)
        struct {
            char*    enum_name;
            int      enum_name_length;
            char*    value_name;
            int      value_name_length;
            NodeList args;           // constructor arg expressions (count==0 for bare tag)
            int      has_parens;     // set by parser: 1 if () was present (even if empty)
            int      is_data_enum;   // set by checker: 1 if parent enum has data variants
            int      is_module_call; // set by checker: 1 if this is module::func access
        } enum_value;

        // New expression: new Type { fields } or new Type(args)
        struct {
            Node*    type_node;     // NODE_IDENT or NODE_GENERIC_TYPE
            Node*    init;          // NODE_STRUCT_INIT (field form) or NULL (init-call form)
            NodeList args;          // Arguments for init call (when init is NULL)
            Type*    resolved_type; // Set by checker
        } new_expr;

        // String interpolation: $"text {expr} text"
        struct {
            NodeList parts;      // Alternating NODE_STRING_LIT and expression nodes
            Type**   part_types; // Set by checker: type of each part (NULL for text)
            int      part_count; // Number of parts
        } string_interp;

        // Cast expression: expr as Type
        struct {
            Node* expr;
            Node* type_node;
            Type* resolved_type; // Set by checker
        } cast_expr;

        // Try expression: expr?
        struct {
            Node* expr;
            Type* resolved_type;  // Set by checker: enum type of operand
            Type* unwrapped_type; // Set by checker: T from Ok(T)/Some(T)
            int   is_option;      // Set by checker: 1=Option, 0=Result
            char* enum_name;      // Set by checker: operand's mangled enum name
            char* ret_enum_name;  // Set by checker: function return's mangled enum name
        } try_expr;

        // Lambda expression: |params| body
        struct {
            NodeList params;       // NODE_PARAM nodes
            Node*    return_type;  // Explicit return type (NULL = inferred)
            Node*    body;         // Block or expression node
            int      is_expr_body; // 1 if body is expression (not block)
            // Set by checker:
            int   lambda_id;     // Unique ID for codegen (__lambda_N)
            Type* resolved_type; // TYPE_FUNC
            // Capture list (set by checker for closures):
            struct {
                char** names; // Captured variable names (owned copies)
                Type** types; // Type of each captured variable (not owned)
                int*   is_rc; // 1 if captured variable is RC-managed
                int    count;
                int    capacity;
            } captures;
        } lambda;

        // Tuple type: (T1, T2, ...)
        struct {
            NodeList elem_types;
        } tuple_type;

        // Tuple literal: (e1, e2, ...)
        struct {
            NodeList elements;
            Type*    resolved_type; // Set by checker (tuple type)
        } tuple_lit;

        // Array literal: [e1, e2, ...]
        struct {
            NodeList elements;
            Type*    resolved_type; // Set by checker (element type)
        } array_lit;

        // Array type: [n]T or []T
        struct {
            Node* elem_type; // Element type
            Node* size;      // Size expression (NULL for unsized/span)
        } array_type;

        // Generic type reference: Box<i64>, Pair<K, V>
        struct {
            char*    base_name;
            int      base_name_length;
            NodeList type_args; // list of type nodes
        } generic_type;

        // Function type: func(T1, T2) -> ReturnType
        struct {
            NodeList param_types; // Parameter type nodes
            Node*    return_type; // Return type node (NULL for void)
        } func_type;

        // Expression statement
        struct {
            Node* expr;
        } expr_stmt;

        // Variable declaration
        var_decl_node var_decl;

        // Block
        struct {
            NodeList stmts;
        } block;

        // If statement
        struct {
            Node* cond;
            Node* then_block;
            Node* else_block;
        } if_stmt;

        // If-is statement: if (expr is Variant(bindings) [&& cond]) { ... } else { ... }
        struct {
            Node*  expr;         // Expression being matched
            char*  variant_name; // Variant to match (e.g., "Ok")
            int    variant_name_length;
            char*  enum_name; // Qualified enum name (NULL if inferred)
            int    enum_name_length;
            char** bindings; // Binding names [f0, f1, ...]
            int    binding_count;
            Node*  then_block;
            Node*  else_block;    // NODE_IF, NODE_IF_LET, or NODE_BLOCK
            Node*  extra_cond;    // Optional && condition after pattern (NULL if none)
            Type*  resolved_type; // Set by checker: enum type of expr
        } if_let_stmt;

        // While statement
        struct {
            Node* cond;
            Node* body;
        } while_stmt;

        // For statement
        struct {
            Node* init;
            Node* cond;
            Node* post;
            Node* body;
        } for_stmt;

        // Foreach statement
        struct {
            char* var_name;
            int   var_name_length;
            Node* start;
            Node* end;
            Node* step;
            Node* body;
            Node* collection;    // Non-NULL for collection foreach (e.g. Vec<T>)
            Type* resolved_type; // Set by checker (loop variable type)
            int   is_span;       // Set by checker if collection is Span<T>
            int   is_string;     // Set by checker if collection is string
        } foreach_stmt;

        // Return statement
        struct {
            Node* value;
        } return_stmt;

        // Defer statement
        struct {
            Node* stmt;
        } defer_stmt;

        // Match statement
        struct {
            Node*    expr;                // Expression being matched
            NodeList arms;                // List of NODE_MATCH_ARM nodes
            Type*    resolved_type;       // Set by checker: enum type of expr
            Type*    resolved_value_type; // Set by checker for match-expr result type
            int      is_value_match;      // Set by checker: 1 if matching on non-enum type
        } match_stmt;

        // Match arm
        struct {
            char*  enum_name; // Explicit enum name (NULL if inferred)
            int    enum_name_length;
            char*  variant_name; // Variant name (NULL if wildcard)
            int    variant_name_length;
            char** bindings; // Binding names [f0_name, f1_name, ...]
            int    binding_count;
            int    is_wildcard;  // 1 if this is a `_` arm
            Node*  body;         // Statement/block (match stmt) or expression (match expr)
            Node*  pattern_expr; // Literal pattern for value match (NULL for enum match)
        } match_arm;

        // Function declaration
        func_decl_node func_decl;

        // Parameter
        struct {
            char* name;
            int   name_length;
            Node* type;
            int   is_const;
        } param;

        // Struct declaration
        struct {
            int      is_public;
            char*    name;
            int      name_length;
            NodeList fields;
            char*    source_module; // NULL = same module, else external module name
            // Generic type parameters (NULL if not generic)
            char** type_params;       // ["T"] or ["K", "V"]
            char** type_param_bounds; // Trait bounds parallel to type_params (NULL entries =
                                      // unbounded)
            int type_param_count;
        } struct_decl;

        // Field
        struct {
            char* name;
            int   name_length;
            Node* type;
            int   is_const;
            int   is_private;
        } field;

        // Enum declaration
        struct {
            int      is_public;
            char*    name;
            int      name_length;
            NodeList values;        // list of NODE_ENUM_VARIANT nodes
            char*    source_module; // NULL = same module, else external module name
            // Generic type parameters (NULL if not generic)
            char** type_params;       // ["T"] or ["T", "E"]
            char** type_param_bounds; // Trait bounds parallel to type_params (NULL = unbounded)
            int    type_param_count;
        } enum_decl;

        // Enum variant (inside enum declaration)
        struct {
            char*    name;
            int      name_length;
            NodeList types; // payload type nodes (empty for simple variants)
            int      has_explicit_value;
            long     explicit_value;
        } enum_variant;

        // Trait declaration
        struct {
            int      is_public;
            char*    name;
            int      name_length;
            NodeList methods; // List of func_decl nodes (signatures only, no body)
            char*    source_module;
        } trait_decl;

        // Impl block: impl Trait for Type { methods }
        struct {
            char*    trait_name;
            int      trait_name_length;
            char*    type_name;
            int      type_name_length;
            NodeList type_args; // Type args for generic target (e.g., <T> from Box<T>)
            NodeList methods;   // List of func_decl nodes (with bodies and receivers)
            char*    source_module;
        } impl_decl;

        // Type alias: type Name = TargetType; or type Name<T> = GenericType<T>;
        struct {
            int    is_public;
            char*  name;
            int    name_length;
            Node*  target_type; // The RHS type expression
            char*  source_module;
            char** type_params;       // Generic params: ["T", "V"]
            char** type_param_bounds; // Trait bounds (parallel array)
            int    type_param_count;
        } type_alias;

        // Use declaration: use module.symbol or use module.{sym1, sym2}
        struct {
            char*  module_name;
            int    module_name_length;
            char** symbol_names;
            int*   symbol_name_lengths;
            int    symbol_count;
        } use_decl;

        // Test block: test "name" { body }
        struct {
            char* name;
            int   name_length;
            Node* body;
        } test_decl;

        struct {
            char*    module_name;
            int      module_name_length;
            NodeList decls;
        } extern_module;

        // Module
        struct {
            char*    name;
            int      name_length;
            NodeList decls;
        } module;

        // Program
        struct {
            NodeList modules;
        } program;
    } as;
};

// Node creation
Node* node_new(NodeType type, int line, int column);
void  node_free(Node* node);
Node* node_clone(Node* node);

// NodeList operations
void nodelist_init(NodeList* list);
void nodelist_push(NodeList* list, Node* node);
void nodelist_free(NodeList* list);

// Match helpers
int match_stmt_has_wildcard_arm(Node* node);

// Owned temp predicates
int has_owned_temps(Node* node);

// AST query functions
Node* find_generic_struct_decl(Node* ast, const char* name);
Node* find_generic_enum_decl(Node* ast, const char* name);
Node* find_generic_func_decl(Node* ast, const char* name);
Node* find_generic_method_func_decl(Node* ast, const char* receiver_type, const char* method_name);
void  collect_generic_methods(Node* ast, const char* struct_name, Node*** methods_out,
                              int* count_out);

#endif
