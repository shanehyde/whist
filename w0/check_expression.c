#include "check_expression.h"

#include <string.h>

#include "checker_util.h"

Type* check_expression(Checker* checker, Node* node) {
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
            check_error(checker, node->line, node->column, "Undefined identifier '%s'",
                        node->as.ident.name);
            return type_error;
        }
        return sym->type;
    }

    case NODE_ENUM_VALUE: {
        // Look up the enum type
        Symbol* sym = checker_lookup(checker, node->as.enum_value.enum_name);
        if (!sym || sym->kind != SYM_TYPE) {
            check_error(checker, node->line, node->column, "Unknown enum '%s'",
                        node->as.enum_value.enum_name);
            return type_error;
        }
        Type* enum_type = sym->type;
        if (enum_type->kind != TYPE_ENUM) {
            check_error(checker, node->line, node->column, "'%s' is not an enum",
                        node->as.enum_value.enum_name);
            return type_error;
        }
        // Check that the value exists in the enum
        int found = 0;
        for (int i = 0; i < enum_type->as.enm.value_count; i++) {
            if (strcmp(enum_type->as.enm.value_names[i], node->as.enum_value.value_name) == 0) {
                found = 1;
                break;
            }
        }
        if (!found) {
            check_error(checker, node->line, node->column, "'%s' is not a value of enum '%s'",
                        node->as.enum_value.value_name, node->as.enum_value.enum_name);
            return type_error;
        }
        return enum_type;
    }

    case NODE_BINARY: {
        Type* left  = check_expression(checker, node->as.binary.left);
        Type* right = check_expression(checker, node->as.binary.right);

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
            check_error(checker, node->line, node->column, "Cannot compare '%s' and '%s'",
                        type_name(left), type_name(right));
            return type_error;
        }

        // Logical operators
        if (op == TOK_AMP_AMP || op == TOK_PIPE_PIPE) {
            if (left->kind != TYPE_BOOL || right->kind != TYPE_BOOL) {
                check_error(checker, node->line, node->column,
                            "Logical operators require bool operands");
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
                // Otherwise default to i64
                return type_int64;
            }

            // Pointer arithmetic
            if (left->kind == TYPE_POINTER && type_is_integer(right)) {
                return left;
            }

            check_error(checker, node->line, node->column,
                        "Invalid operands to '%s': '%s' and '%s'", token_type_name(op),
                        type_name(left), type_name(right));
            return type_error;
        }

        // Bitwise operators
        if (op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET || op == TOK_LT_LT ||
            op == TOK_GT_GT) {
            if (!type_is_integer(left) || !type_is_integer(right)) {
                check_error(checker, node->line, node->column,
                            "Bitwise operators require integer operands");
                return type_error;
            }
            // Return common type or promote to i64
            if (type_equals(left, right)) {
                return left;
            }
            return type_int64;
        }

        check_error(checker, node->line, node->column, "Unknown binary operator");
        return type_error;
    }

    case NODE_UNARY: {
        Type*     operand = check_expression(checker, node->as.unary.operand);
        TokenType op      = node->as.unary.op;

        if (operand->kind == TYPE_ERROR)
            return type_error;

        switch (op) {
        case TOK_MINUS:
            if (!type_is_integer(operand) && operand->kind != TYPE_F32 &&
                operand->kind != TYPE_F64) {
                check_error(checker, node->line, node->column,
                            "Unary '-' requires numeric operand");
                return type_error;
            }
            return operand;

        case TOK_BANG:
            if (operand->kind != TYPE_BOOL) {
                check_error(checker, node->line, node->column, "Unary '!' requires bool operand");
                return type_error;
            }
            return type_bool;

        case TOK_TILDE:
            if (!type_is_integer(operand)) {
                check_error(checker, node->line, node->column,
                            "Unary '~' requires integer operand");
                return type_error;
            }
            return operand;

        case TOK_AMP:
            // Address-of
            return type_pointer(operand);

        case TOK_STAR:
            // Dereference
            if (operand->kind != TYPE_POINTER) {
                check_error(checker, node->line, node->column,
                            "Cannot dereference non-pointer type '%s'", type_name(operand));
                return type_error;
            }
            return operand->as.pointer.inner ? operand->as.pointer.inner : type_error;

        case TOK_PLUS_PLUS:
        case TOK_MINUS_MINUS:
            if (!type_is_integer(operand) && operand->kind != TYPE_POINTER) {
                check_error(checker, node->line, node->column,
                            "Increment/decrement requires integer or pointer");
                return type_error;
            }
            return operand;

        default:
            check_error(checker, node->line, node->column, "Unknown unary operator");
            return type_error;
        }
    }

    case NODE_CALL: {
        Type* func_type = check_expression(checker, node->as.call.func);

        if (func_type->kind == TYPE_ERROR)
            return type_error;

        if (func_type->kind != TYPE_FUNC) {
            check_error(checker, node->line, node->column, "Cannot call non-function type '%s'",
                        type_name(func_type));
            return type_error;
        }

        // Check argument count
        if (node->as.call.args.count != func_type->as.func.param_count) {
            check_error(checker, node->line, node->column, "Expected %d arguments, got %d",
                        func_type->as.func.param_count, node->as.call.args.count);
            return type_error;
        }

        // Check argument types
        for (int i = 0; i < node->as.call.args.count; i++) {
            Type* arg_type   = check_expression(checker, node->as.call.args.nodes[i]);
            Type* param_type = func_type->as.func.param_types[i];

            if (!type_assignable(param_type, arg_type)) {
                check_error(checker, node->as.call.args.nodes[i]->line,
                            node->as.call.args.nodes[i]->column,
                            "Argument %d: expected '%s', got '%s'", i + 1, type_name(param_type),
                            type_name(arg_type));
            }
        }

        return func_type->as.func.return_type;
    }

    case NODE_INDEX: {
        Type* object = check_expression(checker, node->as.index.object);
        Type* index  = check_expression(checker, node->as.index.index);

        if (object->kind == TYPE_ERROR || index->kind == TYPE_ERROR) {
            return type_error;
        }

        if (!type_is_integer(index)) {
            check_error(checker, node->line, node->column,
                        "Array index must be an integer, got '%s'", type_name(index));
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

        check_error(checker, node->line, node->column, "Cannot index type '%s'", type_name(object));
        return type_error;
    }

    case NODE_MEMBER: {
        Type* object      = check_expression(checker, node->as.member.object);
        Type* struct_type = object;

        if (object->kind == TYPE_ERROR)
            return type_error;

        // Handle -> operator
        if (node->as.member.arrow) {
            if (object->kind != TYPE_POINTER) {
                check_error(checker, node->line, node->column,
                            "'->' requires pointer type, got '%s'", type_name(object));
                return type_error;
            }
            struct_type = object->as.pointer.inner;
            if (!struct_type)
                return type_error;
        }

        if (struct_type->kind != TYPE_STRUCT) {
            check_error(checker, node->line, node->column,
                        "Member access requires struct type, got '%s'", type_name(struct_type));
            return type_error;
        }

        // Find field first
        const char* member_name = node->as.member.name;
        for (int i = 0; i < struct_type->as.struc.field_count; i++) {
            if (strcmp(struct_type->as.struc.field_names[i], member_name) == 0) {
                node->as.member.struct_name = NULL; // Not a method
                return struct_type->as.struc.field_types[i];
            }
        }

        // If not a field, check for method
        for (int i = 0; i < struct_type->as.struc.method_count; i++) {
            if (strcmp(struct_type->as.struc.method_names[i], member_name) == 0) {
                // Store struct name for codegen to use
                node->as.member.struct_name = strdup(struct_type->as.struc.name);
                // Return the method's function type
                return struct_type->as.struc.method_types[i];
            }
        }

        check_error(checker, node->line, node->column, "Struct '%s' has no field or method '%s'",
                    struct_type->as.struc.name, member_name);
        return type_error;
    }

    case NODE_ASSIGN: {
        Type* target = check_expression(checker, node->as.assign.target);
        Type* value  = NULL;

        if (node->as.assign.value && node->as.assign.value->type == NODE_STRUCT_INIT) {
            check_error(checker, node->line, node->column,
                        "Struct initializers are only allowed in variable declarations");
            return type_error;
        }

        value = check_expression(checker, node->as.assign.value);

        if (target->kind == TYPE_ERROR || value->kind == TYPE_ERROR) {
            return type_error;
        }

        // Check if target is assignable (lvalue check could be more thorough)
        Node* t = node->as.assign.target;
        if (t->type == NODE_IDENT) {
            Symbol* sym = checker_lookup(checker, t->as.ident.name);
            if (sym && sym->is_const) {
                check_error(checker, node->line, node->column, "Cannot assign to const '%s'",
                            t->as.ident.name);
                return type_error;
            }
        } else if (t->type == NODE_MEMBER) {
            // Check if assigning through a const pointer (e.g., self->x in a const method)
            Node* obj = t->as.member.object;
            if (obj->type == NODE_IDENT) {
                Symbol* sym = checker_lookup(checker, obj->as.ident.name);
                if (sym && sym->is_const) {
                    check_error(checker, node->line, node->column,
                                "Cannot modify field '%s' through const '%s'", t->as.member.name,
                                obj->as.ident.name);
                    return type_error;
                }
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
                check_error(checker, node->line, node->column,
                            "Invalid operands for compound assignment");
                return type_error;
            }
        }

        if (!type_assignable(target, value)) {
            check_error(checker, node->line, node->column, "Cannot assign '%s' to '%s'",
                        type_name(value), type_name(target));
            return type_error;
        }

        return target;
    }

    case NODE_STRUCT_INIT:
        check_error(checker, node->line, node->column,
                    "Struct initializer requires a contextual struct type");
        return type_error;

    default:
        check_error(checker, node->line, node->column, "Unknown expression type %d", node->type);
        return type_error;
    }
}
