#include "checker.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SCOPE_SIZE 64

static void error(Checker* checker, int line, int col, const char* fmt, ...) {
    checker->error_count++;
    va_list args;
    va_start(args, fmt);
    fprintf(stderr, "[line %d:%d] Error: ", line, col);
    vfprintf(stderr, fmt, args);
    fprintf(stderr, "\n");
    va_end(args);
}

static unsigned int hash_string(const char* str) {
    unsigned int hash = 5381;
    int          c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash;
}

void checker_init(Checker* checker) {
    checker->scope               = NULL;
    checker->current_func_return = NULL;
    checker->in_loop             = 0;
    checker->error_count         = 0;
    checker->error_msg[0]        = '\0';
    types_init();
}

void checker_free(Checker* checker) {
    while (checker->scope) {
        checker_pop_scope(checker);
    }
    types_cleanup();
}

void checker_push_scope(Checker* checker) {
    Scope* scope   = calloc(1, sizeof(Scope));
    scope->symbols = calloc(SCOPE_SIZE, sizeof(Symbol*));
    scope->size    = SCOPE_SIZE;
    scope->parent  = checker->scope;
    checker->scope = scope;
}

void checker_pop_scope(Checker* checker) {
    Scope* scope = checker->scope;
    if (!scope)
        return;

    checker->scope = scope->parent;

    for (int i = 0; i < scope->size; i++) {
        Symbol* sym = scope->symbols[i];
        while (sym) {
            Symbol* next = sym->next;
            free(sym->name);
            free(sym);
            sym = next;
        }
    }
    free(scope->symbols);
    free(scope);
}

Symbol* checker_define(Checker* checker, const char* name, SymbolKind kind, Type* type) {
    Scope*       scope = checker->scope;
    unsigned int index = hash_string(name) % scope->size;

    // Check for redefinition in current scope
    for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return NULL; // Already defined
        }
    }

    Symbol* sym           = calloc(1, sizeof(Symbol));
    sym->kind             = kind;
    sym->name             = strdup(name);
    sym->type             = type;
    sym->is_const         = 0;
    sym->next             = scope->symbols[index];
    scope->symbols[index] = sym;
    return sym;
}

Symbol* checker_lookup(Checker* checker, const char* name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                return sym;
            }
        }
    }
    return NULL;
}

Symbol* checker_lookup_local(Checker* checker, const char* name) {
    Scope* scope = checker->scope;
    if (!scope)
        return NULL;

    unsigned int index = hash_string(name) % scope->size;
    for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return sym;
        }
    }
    return NULL;
}

// Forward declarations
static Type* check_expr(Checker* checker, Node* node);
static void  check_stmt(Checker* checker, Node* node);
static void  check_decl(Checker* checker, Node* node);

static Type* resolve_type(Checker* checker, Node* type_node) {
    if (!type_node)
        return type_void;

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;
        if (strcmp(name, "void") == 0)
            return type_void;
        if (strcmp(name, "bool") == 0)
            return type_bool;
        if (strcmp(name, "int64") == 0)
            return type_int64;
        if (strcmp(name, "int8") == 0)
            return type_int8;
        if (strcmp(name, "int16") == 0)
            return type_int16;
        if (strcmp(name, "int32") == 0)
            return type_int32;
        if (strcmp(name, "uint64") == 0)
            return type_uint64;
        if (strcmp(name, "uint8") == 0)
            return type_uint8;
        if (strcmp(name, "uint16") == 0)
            return type_uint16;
        if (strcmp(name, "uint32") == 0)
            return type_uint32;
        if (strcmp(name, "f32") == 0)
            return type_f32;
        if (strcmp(name, "f64") == 0)
            return type_f64;
        if (strcmp(name, "char") == 0)
            return type_char;
        if (strcmp(name, "string") == 0)
            return type_string;

        // Look up user-defined type
        Symbol* sym = checker_lookup(checker, name);
        if (sym && sym->kind == SYM_TYPE) {
            return sym->type;
        }
        error(checker, type_node->line, type_node->column, "Unknown type '%s'", name);
        return type_error;
    }
    case NODE_UNARY:
        // Pointer type: *T
        if (type_node->as.unary.op == TOK_STAR) {
            Type* inner = resolve_type(checker, type_node->as.unary.operand);
            return type_pointer(inner);
        }
        break;
    case NODE_INDEX:
        // Array type: [n]T
        {
            Type* elem = resolve_type(checker, type_node->as.index.object);
            int   size = -1;
            if (type_node->as.index.index) {
                // For now, only support constant integer sizes
                if (type_node->as.index.index->type == NODE_INT_LIT) {
                    size = (int)type_node->as.index.index->as.int_lit.value;
                }
            }
            return type_array(elem, size);
        }
    default:
        break;
    }

    error(checker, type_node->line, type_node->column, "Invalid type");
    return type_error;
}

static Type* check_expr(Checker* checker, Node* node) {
    if (!node)
        return type_error;

    switch (node->type) {
    case NODE_INT_LIT:
        return type_int64;

    case NODE_FLOAT_LIT:
        return type_f32;

    case NODE_STRING_LIT:
        return type_string;

    case NODE_CHAR_LIT:
        return type_char;

    case NODE_BOOL_LIT:
        return type_bool;

    case NODE_NULL_LIT:
        return type_pointer(NULL); // null pointer

    case NODE_IDENT: {
        Symbol* sym = checker_lookup(checker, node->as.ident.name);
        if (!sym) {
            error(checker, node->line, node->column, "Undefined identifier '%s'",
                  node->as.ident.name);
            return type_error;
        }
        return sym->type;
    }

    case NODE_BINARY: {
        Type* left  = check_expr(checker, node->as.binary.left);
        Type* right = check_expr(checker, node->as.binary.right);

        if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
            return type_error;
        }

        TokenType op = node->as.binary.op;

        // Comparison operators return bool
        if (op == TOK_EQ_EQ || op == TOK_BANG_EQ || op == TOK_LT || op == TOK_GT ||
            op == TOK_LT_EQ || op == TOK_GT_EQ) {
            // Allow comparing same types or numeric types
            if (type_equals(left, right))
                return type_bool;
            if ((type_is_integer(left) || left->kind == TYPE_F32 || left->kind == TYPE_F64) &&
                (type_is_integer(right) || right->kind == TYPE_F32 || right->kind == TYPE_F64)) {
                return type_bool;
            }
            error(checker, node->line, node->column, "Cannot compare '%s' and '%s'",
                  type_name(left), type_name(right));
            return type_error;
        }

        // Logical operators
        if (op == TOK_AMP_AMP || op == TOK_PIPE_PIPE) {
            if (left->kind != TYPE_BOOL || right->kind != TYPE_BOOL) {
                error(checker, node->line, node->column, "Logical operators require bool operands");
                return type_error;
            }
            return type_bool;
        }

        // Arithmetic operators
        if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH ||
            op == TOK_PERCENT) {
            // Numeric operands
            if ((type_is_integer(left) || left->kind == TYPE_F32 || left->kind == TYPE_F64) &&
                (type_is_integer(right) || right->kind == TYPE_F32 || right->kind == TYPE_F64)) {
                // Promote to float if either operand is f32/f64
                if (left->kind == TYPE_F64 || right->kind == TYPE_F64) {
                    return type_f64;
                }
                if (left->kind == TYPE_F32 || right->kind == TYPE_F32) {
                    return type_f32;
                }
                // For integer types, return the larger/common type
                // If they're the same type, return that type
                if (type_equals(left, right)) {
                    return left;
                }
                // Otherwise default to int64
                return type_int64;
            }

            // Pointer arithmetic
            if (left->kind == TYPE_POINTER && type_is_integer(right)) {
                return left;
            }

            error(checker, node->line, node->column, "Invalid operands to '%s': '%s' and '%s'",
                  token_type_name(op), type_name(left), type_name(right));
            return type_error;
        }

        // Bitwise operators
        if (op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET || op == TOK_LT_LT ||
            op == TOK_GT_GT) {
            if (!type_is_integer(left) || !type_is_integer(right)) {
                error(checker, node->line, node->column,
                      "Bitwise operators require integer operands");
                return type_error;
            }
            // Return common type or promote to int64
            if (type_equals(left, right)) {
                return left;
            }
            return type_int64;
        }

        error(checker, node->line, node->column, "Unknown binary operator");
        return type_error;
    }

    case NODE_UNARY: {
        Type*     operand = check_expr(checker, node->as.unary.operand);
        TokenType op      = node->as.unary.op;

        if (operand->kind == TYPE_ERROR)
            return type_error;

        switch (op) {
        case TOK_MINUS:
            if (!type_is_integer(operand) && operand->kind != TYPE_F32 &&
                operand->kind != TYPE_F64) {
                error(checker, node->line, node->column, "Unary '-' requires numeric operand");
                return type_error;
            }
            return operand;

        case TOK_BANG:
            if (operand->kind != TYPE_BOOL) {
                error(checker, node->line, node->column, "Unary '!' requires bool operand");
                return type_error;
            }
            return type_bool;

        case TOK_TILDE:
            if (!type_is_integer(operand)) {
                error(checker, node->line, node->column, "Unary '~' requires integer operand");
                return type_error;
            }
            return operand;

        case TOK_AMP:
            // Address-of
            return type_pointer(operand);

        case TOK_STAR:
            // Dereference
            if (operand->kind != TYPE_POINTER) {
                error(checker, node->line, node->column, "Cannot dereference non-pointer type '%s'",
                      type_name(operand));
                return type_error;
            }
            return operand->as.pointer.inner ? operand->as.pointer.inner : type_error;

        case TOK_PLUS_PLUS:
        case TOK_MINUS_MINUS:
            if (!type_is_integer(operand) && operand->kind != TYPE_POINTER) {
                error(checker, node->line, node->column,
                      "Increment/decrement requires integer or pointer");
                return type_error;
            }
            return operand;

        default:
            error(checker, node->line, node->column, "Unknown unary operator");
            return type_error;
        }
    }

    case NODE_CALL: {
        Type* func_type = check_expr(checker, node->as.call.func);

        if (func_type->kind == TYPE_ERROR)
            return type_error;

        if (func_type->kind != TYPE_FUNC) {
            error(checker, node->line, node->column, "Cannot call non-function type '%s'",
                  type_name(func_type));
            return type_error;
        }

        // Check argument count
        if (node->as.call.args.count != func_type->as.func.param_count) {
            error(checker, node->line, node->column, "Expected %d arguments, got %d",
                  func_type->as.func.param_count, node->as.call.args.count);
            return type_error;
        }

        // Check argument types
        for (int i = 0; i < node->as.call.args.count; i++) {
            Type* arg_type   = check_expr(checker, node->as.call.args.nodes[i]);
            Type* param_type = func_type->as.func.param_types[i];

            if (!type_assignable(param_type, arg_type)) {
                error(checker, node->as.call.args.nodes[i]->line,
                      node->as.call.args.nodes[i]->column, "Argument %d: expected '%s', got '%s'",
                      i + 1, type_name(param_type), type_name(arg_type));
            }
        }

        return func_type->as.func.return_type;
    }

    case NODE_INDEX: {
        Type* object = check_expr(checker, node->as.index.object);
        Type* index  = check_expr(checker, node->as.index.index);

        if (object->kind == TYPE_ERROR || index->kind == TYPE_ERROR) {
            return type_error;
        }

        if (!type_is_integer(index)) {
            error(checker, node->line, node->column, "Array index must be an integer, got '%s'",
                  type_name(index));
            return type_error;
        }

        if (object->kind == TYPE_ARRAY) {
            return object->as.array.elem;
        }
        if (object->kind == TYPE_POINTER) {
            return object->as.pointer.inner ? object->as.pointer.inner : type_error;
        }
        if (object->kind == TYPE_STRING) {
            return type_char;
        }

        error(checker, node->line, node->column, "Cannot index type '%s'", type_name(object));
        return type_error;
    }

    case NODE_MEMBER: {
        Type* object = check_expr(checker, node->as.member.object);

        if (object->kind == TYPE_ERROR)
            return type_error;

        // Handle -> operator
        if (node->as.member.arrow) {
            if (object->kind != TYPE_POINTER) {
                error(checker, node->line, node->column, "'->' requires pointer type, got '%s'",
                      type_name(object));
                return type_error;
            }
            object = object->as.pointer.inner;
            if (!object)
                return type_error;
        }

        if (object->kind != TYPE_STRUCT) {
            error(checker, node->line, node->column, "Member access requires struct type, got '%s'",
                  type_name(object));
            return type_error;
        }

        // Find field
        const char* field_name = node->as.member.name;
        for (int i = 0; i < object->as.struc.field_count; i++) {
            if (strcmp(object->as.struc.field_names[i], field_name) == 0) {
                return object->as.struc.field_types[i];
            }
        }

        error(checker, node->line, node->column, "Struct '%s' has no field '%s'",
              object->as.struc.name, field_name);
        return type_error;
    }

    case NODE_ASSIGN: {
        Type* target = check_expr(checker, node->as.assign.target);
        Type* value  = check_expr(checker, node->as.assign.value);

        if (target->kind == TYPE_ERROR || value->kind == TYPE_ERROR) {
            return type_error;
        }

        // Check if target is assignable (lvalue check could be more thorough)
        Node* t = node->as.assign.target;
        if (t->type == NODE_IDENT) {
            Symbol* sym = checker_lookup(checker, t->as.ident.name);
            if (sym && sym->is_const) {
                error(checker, node->line, node->column, "Cannot assign to const '%s'",
                      t->as.ident.name);
                return type_error;
            }
        }

        // For compound assignment, check operation is valid
        TokenType op = node->as.assign.op;
        if (op != TOK_EQ) {
            // Compound assignment: +=, -=, etc.
            // Check types are compatible for arithmetic
            if ((!type_is_integer(target) && target->kind != TYPE_F32 &&
                 target->kind != TYPE_F64) ||
                (!type_is_integer(value) && value->kind != TYPE_F32 && value->kind != TYPE_F64)) {
                error(checker, node->line, node->column,
                      "Invalid operands for compound assignment");
                return type_error;
            }
        }

        if (!type_assignable(target, value)) {
            error(checker, node->line, node->column, "Cannot assign '%s' to '%s'", type_name(value),
                  type_name(target));
            return type_error;
        }

        return target;
    }

    default:
        error(checker, node->line, node->column, "Unknown expression type %d", node->type);
        return type_error;
    }
}

static void check_stmt(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT:
        check_expr(checker, node->as.expr_stmt.expr);
        break;

    case NODE_VAR_DECL: {
        const char* name = node->as.var_decl.name;

        // Check for redefinition
        if (checker_lookup_local(checker, name)) {
            error(checker, node->line, node->column, "Redefinition of '%s'", name);
            return;
        }

        Type* decl_type = NULL;
        Type* init_type = NULL;

        if (node->as.var_decl.type) {
            decl_type = resolve_type(checker, node->as.var_decl.type);
        }

        if (node->as.var_decl.init) {
            init_type = check_expr(checker, node->as.var_decl.init);
        }

        Type* var_type;
        if (decl_type && init_type) {
            // Both specified - check compatibility
            if (!type_assignable(decl_type, init_type)) {
                error(checker, node->line, node->column,
                      "Cannot initialize '%s' of type '%s' with '%s'", name, type_name(decl_type),
                      type_name(init_type));
            }
            var_type = decl_type;
        } else if (decl_type) {
            var_type = decl_type;
        } else if (init_type) {
            var_type = init_type;
        } else {
            error(checker, node->line, node->column,
                  "Variable '%s' needs type annotation or initializer", name);
            var_type = type_error;
        }

        Symbol* sym = checker_define(checker, name, SYM_VAR, var_type);
        if (sym) {
            sym->is_const = node->as.var_decl.is_const;
        }
        break;
    }

    case NODE_BLOCK:
        checker_push_scope(checker);
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            check_stmt(checker, node->as.block.stmts.nodes[i]);
        }
        checker_pop_scope(checker);
        break;

    case NODE_IF: {
        Type* cond = check_expr(checker, node->as.if_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            error(checker, node->as.if_stmt.cond->line, node->as.if_stmt.cond->column,
                  "If condition must be bool, got '%s'", type_name(cond));
        }
        check_stmt(checker, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_block) {
            check_stmt(checker, node->as.if_stmt.else_block);
        }
        break;
    }

    case NODE_WHILE: {
        Type* cond = check_expr(checker, node->as.while_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            error(checker, node->as.while_stmt.cond->line, node->as.while_stmt.cond->column,
                  "While condition must be bool, got '%s'", type_name(cond));
        }
        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_stmt(checker, node->as.while_stmt.body);
        checker->in_loop = was_in_loop;
        break;
    }

    case NODE_FOR: {
        checker_push_scope(checker); // New scope for init var

        if (node->as.for_stmt.init) {
            check_stmt(checker, node->as.for_stmt.init);
        }

        if (node->as.for_stmt.cond) {
            Type* cond = check_expr(checker, node->as.for_stmt.cond);
            if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
                error(checker, node->as.for_stmt.cond->line, node->as.for_stmt.cond->column,
                      "For condition must be bool, got '%s'", type_name(cond));
            }
        }

        if (node->as.for_stmt.post) {
            check_expr(checker, node->as.for_stmt.post);
        }

        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_stmt(checker, node->as.for_stmt.body);
        checker->in_loop = was_in_loop;

        checker_pop_scope(checker);
        break;
    }

    case NODE_RETURN: {
        Type* expected = checker->current_func_return;
        if (!expected) {
            error(checker, node->line, node->column, "Return outside of function");
            return;
        }

        if (node->as.return_stmt.value) {
            Type* actual = check_expr(checker, node->as.return_stmt.value);
            if (!type_assignable(expected, actual)) {
                error(checker, node->line, node->column,
                      "Return type mismatch: expected '%s', got '%s'", type_name(expected),
                      type_name(actual));
            }
        } else if (expected->kind != TYPE_VOID) {
            error(checker, node->line, node->column, "Return without value in non-void function");
        }
        break;
    }

    case NODE_BREAK:
        if (!checker->in_loop) {
            error(checker, node->line, node->column, "Break outside of loop");
        }
        break;

    case NODE_CONTINUE:
        if (!checker->in_loop) {
            error(checker, node->line, node->column, "Continue outside of loop");
        }
        break;

    default:
        error(checker, node->line, node->column, "Unknown statement type %d", node->type);
        break;
    }
}

static void check_decl(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_FUNC_DECL: {
        const char* name = node->as.func_decl.name;

        // Check for redefinition
        if (checker_lookup(checker, name)) {
            error(checker, node->line, node->column, "Redefinition of '%s'", name);
            return;
        }

        // Build function type
        int    param_count = node->as.func_decl.params.count;
        Type** param_types = NULL;
        if (param_count > 0) {
            param_types = malloc(param_count * sizeof(Type*));
        }

        Type* return_type = type_void;
        if (node->as.func_decl.return_type) {
            return_type = resolve_type(checker, node->as.func_decl.return_type);
        }

        // Pre-declare function for recursion
        Type* func_type = type_func(param_types, param_count, return_type);
        checker_define(checker, name, SYM_FUNC, func_type);

        // Enter function scope
        checker_push_scope(checker);
        Type* old_return             = checker->current_func_return;
        checker->current_func_return = return_type;

        // Define parameters
        for (int i = 0; i < param_count; i++) {
            Node* param = node->as.func_decl.params.nodes[i];
            Type* ptype = type_void;
            if (param->as.param.type) {
                ptype = resolve_type(checker, param->as.param.type);
            }
            param_types[i] = ptype;

            if (!checker_define(checker, param->as.param.name, SYM_VAR, ptype)) {
                error(checker, param->line, param->column, "Duplicate parameter name '%s'",
                      param->as.param.name);
            }
        }

        // Check body
        if (node->as.func_decl.body) {
            // Body is a block, but we already pushed scope for params
            // So just check the statements directly
            Node* body = node->as.func_decl.body;
            for (int i = 0; i < body->as.block.stmts.count; i++) {
                check_stmt(checker, body->as.block.stmts.nodes[i]);
            }
        }

        checker->current_func_return = old_return;
        checker_pop_scope(checker);
        break;
    }

    case NODE_STRUCT_DECL: {
        const char* name = node->as.struct_decl.name;

        if (checker_lookup(checker, name)) {
            error(checker, node->line, node->column, "Redefinition of type '%s'", name);
            return;
        }

        Type* struct_type = type_struct(name);
        int   field_count = node->as.struct_decl.fields.count;

        struct_type->as.struc.field_count = field_count;
        struct_type->as.struc.field_names = malloc(field_count * sizeof(char*));
        struct_type->as.struc.field_types = malloc(field_count * sizeof(Type*));

        for (int i = 0; i < field_count; i++) {
            Node* field                          = node->as.struct_decl.fields.nodes[i];
            struct_type->as.struc.field_names[i] = strdup(field->as.field.name);
            struct_type->as.struc.field_types[i] = resolve_type(checker, field->as.field.type);
        }

        checker_define(checker, name, SYM_TYPE, struct_type);
        break;
    }

    case NODE_ENUM_DECL: {
        const char* name = node->as.enum_decl.name;

        if (checker_lookup(checker, name)) {
            error(checker, node->line, node->column, "Redefinition of type '%s'", name);
            return;
        }

        Type* enum_type   = type_enum(name);
        int   value_count = node->as.enum_decl.values.count;

        enum_type->as.enm.value_count = value_count;
        enum_type->as.enm.value_names = malloc(value_count * sizeof(char*));

        checker_define(checker, name, SYM_TYPE, enum_type);

        // Define enum values as constants
        for (int i = 0; i < value_count; i++) {
            Node* val                        = node->as.enum_decl.values.nodes[i];
            enum_type->as.enm.value_names[i] = strdup(val->as.ident.name);

            Symbol* sym = checker_define(checker, val->as.ident.name, SYM_ENUM_VALUE, enum_type);
            if (!sym) {
                error(checker, val->line, val->column, "Redefinition of '%s'", val->as.ident.name);
            }
        }
        break;
    }

    case NODE_VAR_DECL:
        // Global variable
        check_stmt(checker, node);
        break;

    default:
        error(checker, node->line, node->column, "Unknown declaration type %d", node->type);
        break;
    }
}

int checker_check(Checker* checker, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM) {
        return 0;
    }

    checker_push_scope(checker); // Global scope

    // First pass: declare all types and functions (for forward references)
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        Node* decl = ast->as.program.decls.nodes[i];
        if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL) {
            check_decl(checker, decl);
        }
    }

    // Second pass: check everything
    for (int i = 0; i < ast->as.program.decls.count; i++) {
        Node* decl = ast->as.program.decls.nodes[i];
        if (decl->type != NODE_STRUCT_DECL && decl->type != NODE_ENUM_DECL) {
            check_decl(checker, decl);
        }
    }

    checker_pop_scope(checker);

    return checker->error_count == 0;
}
