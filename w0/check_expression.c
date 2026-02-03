#include "check_expression.h"

#include <stdlib.h>
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
        return type_null; // null reference

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

        if (object->kind == TYPE_ARRAY) {
            if (!type_is_integer(index)) {
                check_error(checker, node->line, node->column,
                            "Array index must be an integer, got '%s'", type_name(index));
                return type_error;
            }
            return object->as.array.elem;
        }
        if (object->kind == TYPE_STRING) {
            if (!type_is_integer(index)) {
                check_error(checker, node->line, node->column,
                            "String index must be an integer, got '%s'", type_name(index));
                return type_error;
            }
            return type_char;
        }
        if (object->kind == TYPE_TUPLE) {
            // Tuple index must be a compile-time constant integer
            if (node->as.index.index->type != NODE_INT_LIT) {
                check_error(checker, node->line, node->column,
                            "Tuple index must be a compile-time constant");
                return type_error;
            }
            int idx = (int)node->as.index.index->as.int_lit.value;
            if (idx < 0 || idx >= object->as.tuple.elem_count) {
                check_error(checker, node->line, node->column,
                            "Tuple index %d out of bounds (tuple has %d elements)", idx,
                            object->as.tuple.elem_count);
                return type_error;
            }
            // Mark this as a tuple index for codegen
            node->as.index.is_tuple_index = 1;
            return object->as.tuple.elem_types[idx];
        }

        check_error(checker, node->line, node->column, "Cannot index type '%s'", type_name(object));
        return type_error;
    }

    case NODE_MEMBER: {
        // Check for module-qualified access first (e.g., std.print)
        if (node->as.member.object->type == NODE_IDENT) {
            const char* name = node->as.member.object->as.ident.name;
            if (is_imported_module(checker, name)) {
                Symbol* sym = checker_lookup_in_module(checker, name, node->as.member.name);
                if (!sym) {
                    check_error(checker, node->line, node->column,
                                "Module '%s' has no public symbol '%s'", name,
                                node->as.member.name);
                    return type_error;
                }
                node->as.member.module_name = strdup(name);
                return sym->type;
            }
        }

        Type* object = check_expression(checker, node->as.member.object);

        if (object->kind == TYPE_ERROR)
            return type_error;

        // Struct types are always references
        if (object->kind != TYPE_STRUCT) {
            check_error(checker, node->line, node->column,
                        "Member access requires struct type, got '%s'", type_name(object));
            return type_error;
        }

        // Mark as a reference for codegen (struct vars are always refs)
        node->as.member.is_ref = 1;

        // Find field first
        const char* member_name = node->as.member.name;
        for (int i = 0; i < object->as.struc.field_count; i++) {
            if (strcmp(object->as.struc.field_names[i], member_name) == 0) {
                node->as.member.struct_name = NULL; // Not a method
                return object->as.struc.field_types[i];
            }
        }

        // If not a field, check for method
        for (int i = 0; i < object->as.struc.method_count; i++) {
            if (strcmp(object->as.struc.method_names[i], member_name) == 0) {
                // Store struct name for codegen to use
                char* sname = strdup(object->as.struc.name);
                if (!sname) {
                    check_error(checker, node->line, node->column, "Out of memory");
                    return type_error;
                }
                node->as.member.struct_name = sname;
                // Return the method's function type
                return object->as.struc.method_types[i];
            }
        }

        check_error(checker, node->line, node->column, "Struct '%s' has no field or method '%s'",
                    object->as.struc.name, member_name);
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

    case NODE_TUPLE_LIT: {
        int    count = node->as.tuple_lit.elements.count;
        Type** elems = malloc(count * sizeof(Type*));
        for (int i = 0; i < count; i++) {
            elems[i] = check_expression(checker, node->as.tuple_lit.elements.nodes[i]);
        }
        return type_tuple(elems, count);
    }

    default:
        check_error(checker, node->line, node->column, "Unknown expression type %d", node->type);
        return type_error;
    }
}
