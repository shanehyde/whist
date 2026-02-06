#ifndef WHIST_AST_H
#define WHIST_AST_H

#include "lexer.h"

// Forward declaration for destructuring patterns
typedef struct DestructPattern DestructPattern;

// Destructuring pattern kinds
typedef enum {
    PATTERN_IDENT, // Single identifier: x
    PATTERN_TUPLE  // Nested tuple: (a, b) or (a, (b, c))
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
    } as;
    void* resolved_type; // Type* set by checker (cast to void* to avoid dep)
};

// Pattern memory management
DestructPattern* pattern_new_ident(const char* name, int length);
DestructPattern* pattern_new_tuple(int capacity);
void             pattern_tuple_push(DestructPattern* pattern, DestructPattern* elem);
void             pattern_free(DestructPattern* pattern);

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

    // Statements
    NODE_EXPR_STMT,
    NODE_VAR_DECL,
    NODE_BLOCK,
    NODE_IF,
    NODE_WHILE,
    NODE_FOR,
    NODE_FOREACH,
    NODE_RETURN,
    NODE_BREAK,
    NODE_CONTINUE,
    NODE_DEFER,

    // Declarations
    NODE_FUNC_DECL,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,

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
    // For generic method receivers: func (Box<T>) get(): T
    // or func (Pair<i32, Box<T>>) set(): void
    NodeList receiver_type_args; // Type nodes for type arguments in receiver
    char*    name;
    int      name_length;
    NodeList params;
    int      is_varargs;
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
    char* source_module; // NULL = same module, else external module name

    // Destructuring support: var (a, b) = tuple; or var (a, (b, c)) = nested;
    DestructPattern* destruct_pattern; // NULL if not destructuring
    int              is_rc; // Set by checker: 1 if init is a new expression or copy of RC var
    void*            resolved_type; // Type* set by checker for RC vars (the struct type)
} var_decl_node;

struct Node {
    NodeType type;
    int      line;
    int      column;

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
        } ident;

        // Binary expression
        struct {
            TokenType op;
            Node*     left;
            Node*     right;
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
        } call;

        // Index expression: arr[index]
        struct {
            Node* object;
            Node* index;
            int   is_tuple_index; // Set by checker if indexing a tuple
            int   is_span_index;  // Set by checker if indexing a span
        } index;

        // Slice expression: arr[start:end]
        struct {
            Node* object;        // Array or span being sliced
            Node* start;         // Start index (NULL if omitted)
            Node* end;           // End index (NULL if omitted)
            void* resolved_type; // Type* set by checker (cast to void* to avoid dep)
            int   is_array;      // Set by checker: 1 if slicing array, 0 if slicing span
        } slice;

        // Member access: obj.member
        struct {
            Node* object;
            char* name;
            int   length;
            int   is_ref;      // Set by checker: 1 if object is a struct reference
            char* struct_name; // Set by checker if this is a method access (NULL otherwise)
            char* module_name; // Set by checker for module-qualified access (e.g., "std")
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

        // Enum value access: EnumName::ValueName
        struct {
            char* enum_name;
            int   enum_name_length;
            char* value_name;
            int   value_name_length;
        } enum_value;

        // New expression: new Type { fields }
        struct {
            Node* type_node;     // NODE_IDENT or NODE_GENERIC_TYPE
            Node* init;          // NODE_STRUCT_INIT
            void* resolved_type; // Type* set by checker
        } new_expr;

        // Tuple type: (T1, T2, ...)
        struct {
            NodeList elem_types;
        } tuple_type;

        // Tuple literal: (e1, e2, ...)
        struct {
            NodeList elements;
        } tuple_lit;

        // Array literal: [e1, e2, ...]
        struct {
            NodeList elements;
            void*    resolved_type; // Type* set by checker (element type)
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
        } foreach_stmt;

        // Return statement
        struct {
            Node* value;
        } return_stmt;

        // Defer statement
        struct {
            Node* stmt;
        } defer_stmt;

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
            char** type_params; // ["T"] or ["K", "V"]
            int    type_param_count;
        } struct_decl;

        // Field
        struct {
            char* name;
            int   name_length;
            Node* type;
        } field;

        // Enum declaration
        struct {
            int      is_public;
            char*    name;
            int      name_length;
            NodeList values;        // list of ident nodes
            char*    source_module; // NULL = same module, else external module name
        } enum_decl;

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

// NodeList operations
void nodelist_init(NodeList* list);
void nodelist_push(NodeList* list, Node* node);
void nodelist_free(NodeList* list);

#endif
