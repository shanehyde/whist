#ifndef WHIST_AST_H
#define WHIST_AST_H

#include "lexer.h"

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
    NODE_MEMBER,
    NODE_ASSIGN,
    NODE_STRUCT_INIT,
    NODE_FIELD_INIT,
    NODE_ENUM_VALUE,

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

    // Declarations
    NODE_FUNC_DECL,
    NODE_STRUCT_DECL,
    NODE_ENUM_DECL,

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
            int       postfix; // 1 for postfix ++/--, 0 for prefix
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
        } index;

        // Member access: obj.member or obj->member
        struct {
            Node* object;
            char* name;
            int   length;
            int   arrow; // 1 for ->, 0 for .
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

        // Expression statement
        struct {
            Node* expr;
        } expr_stmt;

        // Variable declaration
        struct {
            char* name;
            int   name_length;
            Node* type;
            Node* init;
            int   is_const;
        } var_decl;

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
            Node* body;
        } foreach_stmt;

        // Return statement
        struct {
            Node* value;
        } return_stmt;

        // Function declaration
        struct {
            char*    name;
            int      name_length;
            NodeList params;
            Node*    return_type;
            Node*    body;
        } func_decl;

        // Parameter
        struct {
            char* name;
            int   name_length;
            Node* type;
        } param;

        // Struct declaration
        struct {
            char*    name;
            int      name_length;
            NodeList fields;
        } struct_decl;

        // Field
        struct {
            char* name;
            int   name_length;
            Node* type;
        } field;

        // Enum declaration
        struct {
            char*    name;
            int      name_length;
            NodeList values; // list of ident nodes
        } enum_decl;

        // Program
        struct {
            NodeList decls;
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
