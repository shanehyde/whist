#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "checker_internal.h"
#include "sem_info.h"
#include "vec.h"

// Forward declarations
static Type* check_struct_init(Checker* checker, Node* init, Type* struct_type);
static int   count_captured_boundaries(Checker* checker, const char* name);

// Auto-deref Box<T> to T: if type is TYPE_BOX, set is_box_deref flag and return elem type.
static Type* auto_deref_box(Node* node, Type* type) {
    if (type && type->kind == TYPE_BOX) {
        node->is_box_deref = 1;
        return type->as.box.elem;
    }
    return type;
}

static void infer_type_param(Checker* checker, GenericFuncDef* def, Node* param_type,
                             Type* arg_type, Type** inferred);

static void infer_direct_type_param(GenericFuncDef* def, const char* name, Type* arg_type,
                                    Type** inferred) {
    for (int i = 0; i < def->type_param_count; i++) {
        if (strcmp(name, def->type_params[i]) == 0) {
            if (!inferred[i]) {
                inferred[i] = arg_type;
            }
            return;
        }
    }
}

static void infer_type_arg_list(Checker* checker, GenericFuncDef* def, NodeList* param_type_args,
                                Type** concrete_type_args, int concrete_type_arg_count,
                                Type** inferred) {
    if (param_type_args->count != concrete_type_arg_count) {
        return;
    }

    for (int i = 0; i < param_type_args->count; i++) {
        infer_type_param(checker, def, param_type_args->nodes[i], concrete_type_args[i], inferred);
    }
}

static int infer_from_generic_instance(Checker* checker, GenericFuncDef* def,
                                       NodeList* param_type_args, const char* mangled_name,
                                       Type** inferred) {
    for (int i = 0; i < checker->generics.instance_count; i++) {
        GenericInstance* inst = &checker->generics.instances[i];
        if (strcmp(inst->mangled_name, mangled_name) == 0) {
            infer_type_arg_list(checker, def, param_type_args, inst->type_args,
                                inst->type_arg_count, inferred);
            return 1;
        }
    }
    return 0;
}

// Infer type params from generic type syntax like Vec<T>, Span<T>, or generic struct/enum
// instances.
static void infer_from_generic_param_type(Checker* checker, GenericFuncDef* def, Node* param_type,
                                          Type* arg_type, Type** inferred) {
    const char* base      = param_type->as.generic_type.base_name;
    NodeList*   type_args = &param_type->as.generic_type.type_args;

    if (strcmp(base, "Vec") == 0 && arg_type->kind == TYPE_VEC && type_args->count == 1) {
        infer_type_param(checker, def, type_args->nodes[0], arg_type->as.vec.elem, inferred);
        return;
    }

    if (strcmp(base, "Span") == 0 && arg_type->kind == TYPE_SPAN && type_args->count == 1) {
        infer_type_param(checker, def, type_args->nodes[0], arg_type->as.span.elem, inferred);
        return;
    }

    if (arg_type->kind == TYPE_STRUCT) {
        infer_from_generic_instance(checker, def, type_args, arg_type->as.struc.name, inferred);
        return;
    }
    if (arg_type->kind == TYPE_ENUM) {
        infer_from_generic_instance(checker, def, type_args, arg_type->as.enm.name, inferred);
    }
}

// Infer type params from function type syntax by matching parameter and return types.
static void infer_from_func_param_type(Checker* checker, GenericFuncDef* def, Node* param_type,
                                       Type* arg_type, Type** inferred) {
    if (arg_type->kind != TYPE_FUNC) {
        return;
    }

    infer_type_arg_list(checker, def, &param_type->as.func_type.param_types,
                        arg_type->as.func.param_types, arg_type->as.func.param_count, inferred);
    if (param_type->as.func_type.return_type) {
        infer_type_param(checker, def, param_type->as.func_type.return_type,
                         arg_type->as.func.return_type, inferred);
    }
}

// Infer type params from tuple type syntax by inferring each tuple element pair.
static void infer_from_tuple_param_type(Checker* checker, GenericFuncDef* def, Node* param_type,
                                        Type* arg_type, Type** inferred) {
    if (arg_type->kind != TYPE_TUPLE) {
        return;
    }

    infer_type_arg_list(checker, def, &param_type->as.tuple_type.elem_types,
                        arg_type->as.tuple.elem_types, arg_type->as.tuple.elem_count, inferred);
}

// Infer type params from array-like syntax by accepting both arrays and spans as argument types.
static void infer_from_array_param_type(Checker* checker, GenericFuncDef* def, Node* param_type,
                                        Type* arg_type, Type** inferred) {
    if (arg_type->kind == TYPE_ARRAY) {
        infer_type_param(checker, def, param_type->as.array_type.elem_type, arg_type->as.array.elem,
                         inferred);
    } else if (arg_type->kind == TYPE_SPAN) {
        infer_type_param(checker, def, param_type->as.array_type.elem_type, arg_type->as.span.elem,
                         inferred);
    }
}

// Infer type parameters from a parameter type node and the actual argument type.
// Recursively walks the parameter type AST and binds type parameter names to
// concrete types from the argument. `inferred` array is parallel to def->type_params.
static void infer_type_param(Checker* checker, GenericFuncDef* def, Node* param_type,
                             Type* arg_type, Type** inferred) {
    if (!param_type || !arg_type || arg_type->kind == TYPE_ERROR)
        return;

    switch (param_type->type) {
    case NODE_IDENT:
        infer_direct_type_param(def, param_type->as.ident.name, arg_type, inferred);
        return;
    case NODE_GENERIC_TYPE:
        infer_from_generic_param_type(checker, def, param_type, arg_type, inferred);
        return;
    case NODE_FUNC_TYPE:
        infer_from_func_param_type(checker, def, param_type, arg_type, inferred);
        return;
    case NODE_TUPLE_TYPE:
        infer_from_tuple_param_type(checker, def, param_type, arg_type, inferred);
        return;
    case NODE_ARRAY_TYPE:
        infer_from_array_param_type(checker, def, param_type, arg_type, inferred);
        return;
    default:
        return;
    }
}

// Return the const variable/field name if node is a const binding or const field access, else NULL
static const char* get_const_binding_name(Checker* checker, Node* node) {
    if (node && node->type == NODE_IDENT) {
        Symbol* sym = checker_lookup(checker, node->as.ident.name);
        if (sym && sym->is_const)
            return node->as.ident.name;
    }
    if (node && node->type == NODE_MEMBER &&
        sem_info_get_member_is_const_access(checker->sem, node, node->as.member.is_const_access))
        return node->as.member.name;
    return NULL;
}

// =============================================================================
// Expression checking helpers
// =============================================================================

static int is_equality_op(TokenType op) {
    return op == TOK_EQ_EQ || op == TOK_BANG_EQ;
}

static int is_numeric_comparable_type(Type* type) {
    return type_is_integer(type) || type->kind == TYPE_F32 || type->kind == TYPE_F64;
}

static int is_nullable_comparison_pair(Type* left, Type* right) {
    return (left->kind == TYPE_VOIDPTR && right->kind == TYPE_NULL) ||
           (left->kind == TYPE_NULL && right->kind == TYPE_VOIDPTR) ||
           (left->kind == TYPE_STRUCT && right->kind == TYPE_NULL) ||
           (left->kind == TYPE_NULL && right->kind == TYPE_STRUCT);
}

static int try_check_string_comparison(Node* node, Type* left, Type* right, Type** out_type) {
    if (left->kind != TYPE_STRING || right->kind != TYPE_STRING) {
        return 0;
    }
    node->as.binary.is_string_op = 1;
    *out_type                    = type_bool;
    return 1;
}

static int try_check_vec_comparison(Checker* checker, Node* node, Type* left, Type* right,
                                    TokenType op, Type** out_type) {
    if (left->kind != TYPE_VEC || right->kind != TYPE_VEC) {
        return 0;
    }

    if (!is_equality_op(op)) {
        check_error(checker, node->line, node->column,
                    "Vec types only support == and != comparison");
        *out_type = type_error;
        return 1;
    }

    if (!type_equals(left->as.vec.elem, right->as.vec.elem)) {
        check_error(checker, node->line, node->column,
                    "Cannot compare Vec with different element types");
        *out_type = type_error;
        return 1;
    }

    Type* elem = left->as.vec.elem;
    if (!type_supports_equality(elem)) {
        check_error(checker, node->line, node->column,
                    "Vec element type '%s' does not support ==", type_name(elem));
        *out_type = type_error;
        return 1;
    }

    char buf[256];
    snprintf(buf, sizeof(buf), "__Vec_%s", type_mangle_name(elem));
    node->as.binary.is_eq_op     = 1;
    node->as.binary.eq_type_name = xstrdup(buf);
    *out_type                    = type_bool;
    return 1;
}

static int validate_data_enum_equality(Checker* checker, Node* node, Type* enum_type) {
    for (int v = 0; v < enum_type->as.enm.value_count; v++) {
        for (int f = 0; f < enum_type->as.enm.variant_type_counts[v]; f++) {
            Type* field_type = enum_type->as.enm.variant_types[v][f];
            if (field_type->kind == TYPE_STRUCT && !field_type->as.struc.has_eq) {
                check_error(
                    checker, node->line, node->column,
                    "Cannot compare enum '%s': variant '%s' contains struct '%s' without Eq impl",
                    enum_type->as.enm.name, enum_type->as.enm.value_names[v],
                    field_type->as.struc.name);
                return 0;
            }
        }
    }
    return 1;
}

static Type* check_equal_types_comparison(Checker* checker, Node* node, Type* left, TokenType op) {
    if (left->kind == TYPE_STRUCT && is_equality_op(op)) {
        if (!left->as.struc.has_eq) {
            check_error(checker, node->line, node->column,
                        "Cannot compare struct '%s' with == (no Eq impl)", left->as.struc.name);
            return type_error;
        }
        node->as.binary.is_eq_op     = 1;
        node->as.binary.eq_type_name = left->as.struc.name;
        return type_bool;
    }

    if (left->kind == TYPE_STRUCT) {
        check_error(checker, node->line, node->column, "Cannot compare struct '%s' with '%s'",
                    left->as.struc.name, token_type_symbol(op));
        return type_error;
    }

    if (left->kind == TYPE_ENUM && left->as.enm.has_data && is_equality_op(op)) {
        if (!validate_data_enum_equality(checker, node, left)) {
            return type_error;
        }
        node->as.binary.is_eq_op     = 1;
        node->as.binary.is_enum_eq   = 1;
        node->as.binary.eq_type_name = left->as.enm.name;
        return type_bool;
    }

    return type_bool;
}

// Check comparison operators: == != < > <= >=
static Type* check_comparison_op(Checker* checker, Node* node, Type* left, Type* right) {
    TokenType op = node->as.binary.op;
    Type*     result;

    if (try_check_string_comparison(node, left, right, &result)) {
        return result;
    }
    if (try_check_vec_comparison(checker, node, left, right, op, &result)) {
        return result;
    }

    if (type_equals(left, right)) {
        return check_equal_types_comparison(checker, node, left, op);
    }

    if (is_equality_op(op) && is_nullable_comparison_pair(left, right)) {
        return type_bool;
    }
    if (is_numeric_comparable_type(left) && is_numeric_comparable_type(right)) {
        return type_bool;
    }

    check_error(checker, node->line, node->column, "Cannot compare '%s' and '%s'", type_name(left),
                type_name(right));
    return type_error;
}

// Check logical operators: && ||
static Type* check_logical_op(Checker* checker, Node* node, Type* left, Type* right) {
    if (left->kind != TYPE_BOOL || right->kind != TYPE_BOOL) {
        check_error(checker, node->line, node->column, "Logical operators require bool operands");
        return type_error;
    }
    return type_bool;
}

// Check arithmetic operators: + - * / %
static Type* check_arithmetic_op(Checker* checker, Node* node, Type* left, Type* right) {
    TokenType op = node->as.binary.op;

    // String concatenation: string + string
    if (op == TOK_PLUS && left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
        node->as.binary.is_string_op = 1;
        node->is_owned_temp          = 1;
        node->owned_temp_type        = type_string;
        return type_string;
    }
    if ((type_is_integer(left) || left->kind == TYPE_F32 || left->kind == TYPE_F64) &&
        (type_is_integer(right) || right->kind == TYPE_F32 || right->kind == TYPE_F64)) {
        if (left->kind == TYPE_F64 || right->kind == TYPE_F64)
            return type_f64;
        if (left->kind == TYPE_F32 || right->kind == TYPE_F32)
            return type_f32;
        if (type_equals(left, right))
            return left;
        return type_int64;
    }
    check_error(checker, node->line, node->column, "Invalid operands to '%s': '%s' and '%s'",
                token_type_symbol(op), type_name(left), type_name(right));
    return type_error;
}

// Check bitwise operators: & | ^ << >>
static Type* check_bitwise_op(Checker* checker, Node* node, Type* left, Type* right) {
    if (!type_is_integer(left) || !type_is_integer(right)) {
        check_error(checker, node->line, node->column,
                    "Bitwise operators require integer operands");
        return type_error;
    }
    if (type_equals(left, right))
        return left;
    return type_int64;
}

// Dispatch a binary operator token to the correct operator-specific type checker.
static Type* check_binary_op_by_token(Checker* checker, Node* node, Type* left, Type* right) {
    TokenType op = node->as.binary.op;

    if (op == TOK_EQ_EQ || op == TOK_BANG_EQ || op == TOK_LT || op == TOK_GT || op == TOK_LT_EQ ||
        op == TOK_GT_EQ) {
        return check_comparison_op(checker, node, left, right);
    }
    if (op == TOK_AMP_AMP || op == TOK_PIPE_PIPE) {
        return check_logical_op(checker, node, left, right);
    }
    if (op == TOK_PLUS || op == TOK_MINUS || op == TOK_STAR || op == TOK_SLASH ||
        op == TOK_PERCENT) {
        return check_arithmetic_op(checker, node, left, right);
    }
    if (op == TOK_AMP || op == TOK_PIPE || op == TOK_CARET || op == TOK_LT_LT || op == TOK_GT_GT) {
        return check_bitwise_op(checker, node, left, right);
    }

    check_error(checker, node->line, node->column, "Unknown binary operator");
    return type_error;
}

// Type-check a binary expression: dispatch to operator-specific helpers
static Type* check_binary_expr(Checker* checker, Node* node) {
    Type* left = check_expression(checker, node->as.binary.left);

    // Set enum_target_hint so the right side can infer generic enum params from the left
    // (e.g., v[i] == Option::None where v is Vec<Option<i64>>)
    Type* old_hint = checker->enum_target_hint;
    if (left->kind == TYPE_ENUM) {
        checker->enum_target_hint = left;
    }
    Type* right               = check_expression(checker, node->as.binary.right);
    checker->enum_target_hint = old_hint;

    if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
        return type_error;
    }

    // Auto-deref Box<T> operands
    left  = auto_deref_box(node->as.binary.left, left);
    right = auto_deref_box(node->as.binary.right, right);

    return check_binary_op_by_token(checker, node, left, right);
}

// Type-check a unary expression: negation, logical not, and bitwise complement
static Type* check_unary_expr(Checker* checker, Node* node) {
    Type*     operand = check_expression(checker, node->as.unary.operand);
    TokenType op      = node->as.unary.op;

    if (operand->kind == TYPE_ERROR)
        return type_error;

    // Auto-deref Box<T> operand
    operand = auto_deref_box(node->as.unary.operand, operand);

    switch (op) {
    case TOK_MINUS:
        if (!type_is_integer(operand) && operand->kind != TYPE_F32 && operand->kind != TYPE_F64) {
            check_error(checker, node->line, node->column, "Unary '-' requires numeric operand");
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
            check_error(checker, node->line, node->column, "Unary '~' requires integer operand");
            return type_error;
        }
        return operand;
    default:
        check_error(checker, node->line, node->column, "Unknown unary operator");
        return type_error;
    }
}

// Type-check an index expression: array, span, vec, string, and tuple indexing
static Type* check_index_expr(Checker* checker, Node* node) {
    Type* object = check_expression(checker, node->as.index.object);
    Type* index  = check_expression(checker, node->as.index.index);

    if (object->kind == TYPE_ERROR || index->kind == TYPE_ERROR)
        return type_error;

    if (object->kind == TYPE_ARRAY) {
        if (!type_is_integer(index)) {
            check_error(checker, node->line, node->column,
                        "Array index must be an integer, got '%s'", type_name(index));
            return type_error;
        }
        return object->as.array.elem;
    }
    if (object->kind == TYPE_SPAN) {
        if (!type_is_integer(index)) {
            check_error(checker, node->line, node->column,
                        "Span index must be an integer, got '%s'", type_name(index));
            return type_error;
        }
        node->as.index.is_span_index = 1;
        return object->as.span.elem;
    }
    if (object->kind == TYPE_VEC) {
        if (!type_is_integer(index)) {
            check_error(checker, node->line, node->column, "Vec index must be an integer, got '%s'",
                        type_name(index));
            return type_error;
        }
        node->as.index.is_vec_index = 1;
        Type* elem                  = object->as.vec.elem;
        if (type_is_rc_managed(elem))
            node->as.index.is_rc_elem = 1;
        return elem;
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
        node->as.index.is_tuple_index = 1;
        return object->as.tuple.elem_types[idx];
    }

    check_error_cannot(checker, node->line, node->column, "index", object);
    return type_error;
}

// Type-check a slice expression: validate object is sliceable and bounds are integers
static Type* check_slice_expr(Checker* checker, Node* node) {
    Type* object = check_expression(checker, node->as.slice.object);

    if (object->kind == TYPE_ERROR)
        return type_error;

    // Slicing works on arrays and spans
    Type* elem_type = NULL;
    if (object->kind == TYPE_ARRAY) {
        elem_type               = object->as.array.elem;
        node->as.slice.is_array = 1;
    } else if (object->kind == TYPE_SPAN) {
        elem_type               = object->as.span.elem;
        node->as.slice.is_array = 0;
    } else if (object->kind == TYPE_VEC) {
        elem_type             = object->as.vec.elem;
        node->as.slice.is_vec = 1;
    } else if (object->kind == TYPE_STRING) {
        // String slicing returns a new string
        node->as.slice.is_string = 1;

        // Check bounds are integers (if present)
        if (node->as.slice.start) {
            Type* t = check_expression(checker, node->as.slice.start);
            if (!type_is_integer(t) && t->kind != TYPE_ERROR) {
                check_error(checker, node->as.slice.start->line, node->as.slice.start->column,
                            "Slice start index must be an integer, got '%s'", type_name(t));
            }
        }
        if (node->as.slice.end) {
            Type* t = check_expression(checker, node->as.slice.end);
            if (!type_is_integer(t) && t->kind != TYPE_ERROR) {
                check_error(checker, node->as.slice.end->line, node->as.slice.end->column,
                            "Slice end index must be an integer, got '%s'", type_name(t));
            }
        }
        node->is_owned_temp   = 1;
        node->owned_temp_type = type_string;
        return type_string;
    } else {
        check_error(checker, node->line, node->column, "Cannot slice type '%s'", type_name(object));
        return type_error;
    }

    // Check bounds are integers (if present)
    if (node->as.slice.start) {
        Type* t = check_expression(checker, node->as.slice.start);
        if (!type_is_integer(t) && t->kind != TYPE_ERROR) {
            check_error(checker, node->as.slice.start->line, node->as.slice.start->column,
                        "Slice start index must be an integer, got '%s'", type_name(t));
        }
    }
    if (node->as.slice.end) {
        Type* t = check_expression(checker, node->as.slice.end);
        if (!type_is_integer(t) && t->kind != TYPE_ERROR) {
            check_error(checker, node->as.slice.end->line, node->as.slice.end->column,
                        "Slice end index must be an integer, got '%s'", type_name(t));
        }
    }

    Type* result_type            = type_span(elem_type);
    node->as.slice.resolved_type = result_type;
    return result_type;
}

// --- Member access helpers (dispatched from check_member_expr) ---

// Check module-qualified member access (e.g., std.print, fs.open)

// Check span member access (count, data)
static Type* check_member_span(Checker* checker, Node* node) {
    const char* member_name = node->as.member.name;
    sem_info_set_member_is_ref(checker->sem, node, 0); // Spans are value types, use . not ->
    if (strcmp(member_name, "count") == 0) {
        return type_uint64;
    }
    if (strcmp(member_name, "data") == 0) {
        check_error(checker, node->line, node->column,
                    "Span 'data' field is private; use indexing");
        return type_error;
    }
    check_error(checker, node->line, node->column, "Span has no member '%s'", member_name);
    return type_error;
}

// Check enum member access (.tag and methods)
static Type* check_member_enum(Checker* checker, Node* node, Type* object) {
    const char* member_name = node->as.member.name;
    sem_info_set_member_is_ref(checker->sem, node, 0); // Enums are value types, use . not ->
    // Ensure generic enum methods are instantiated (needed when enum was resolved
    // during Pass 1 before generic methods were registered in Pass 2)
    ensure_generic_enum_methods(checker, object);
    if (object->as.enm.has_data && strcmp(member_name, "tag") == 0) {
        return type_int32;
    }
    for (int i = 0; i < object->as.enm.method_count; i++) {
        if (strcmp(object->as.enm.method_names[i], member_name) == 0) {
            if (!object->as.enm.method_is_const[i]) {
                const char* const_name = get_const_binding_name(checker, node->as.member.object);
                if (const_name) {
                    check_error(checker, node->line, node->column,
                                "Cannot call mutating method '%s' on const '%s'", member_name,
                                const_name);
                    return type_error;
                }
            }
            sem_info_set_member_struct_name(checker->sem, node, object->as.enm.name);
            return object->as.enm.method_types[i];
        }
    }
    check_error(checker, node->line, node->column, "Enum '%s' has no member '%s'",
                object->as.enm.name, member_name);
    return type_error;
}

// Check Vec member access (count, capacity, data, contains, first, last, push, pop, insert,
// remove, swap_remove, clear, reserve, shrink_to_fit, sort)
static void set_vec_member_struct_name(Checker* checker, Node* node, Type* elem_type) {
    char mangled[256];
    snprintf(mangled, sizeof(mangled), "__Vec_%s", type_mangle_name(elem_type));
    sem_info_set_member_struct_name(checker->sem, node, mangled);
}

static int check_mutating_member_on_const(Checker* checker, Node* node, const char* member_name) {
    const char* const_name = get_const_binding_name(checker, node->as.member.object);
    if (!const_name) {
        return 0;
    }
    check_error(checker, node->line, node->column, "Cannot call mutating method '%s' on const '%s'",
                member_name, const_name);
    return 1;
}

static Type* lookup_or_instantiate_option_type(Checker* checker, Type* elem_type) {
    GenericDef* option_def          = lookup_generic_def(checker, "Option");
    Type**      args                = xmalloc(sizeof(Type*));
    args[0]                         = elem_type;
    char*            option_mangled = type_mangle_generic("Option", args, 1);
    GenericInstance* existing       = lookup_generic_instance(checker, option_mangled);
    if (existing) {
        Type* option_type = existing->type;
        free(option_mangled);
        free(args);
        return option_type;
    }
    return instantiate_generic_enum(checker, option_def, option_mangled, args, 1);
}

static Type* check_member_vec_contains(Checker* checker, Node* node, Type* elem_type) {
    if (!type_supports_vec_contains(elem_type)) {
        check_error(checker, node->line, node->column,
                    "Vec.contains requires element type supporting ==, got '%s'",
                    type_name(elem_type));
        return type_error;
    }

    set_vec_member_struct_name(checker, node, elem_type);
    Type** params = xmalloc(sizeof(Type*));
    params[0]     = elem_type;
    return type_func(params, 1, type_bool, 0);
}

static Type* check_member_vec_option_method(Checker* checker, Node* node, Type* elem_type,
                                            int is_mutating) {
    if (is_mutating && check_mutating_member_on_const(checker, node, node->as.member.name)) {
        return type_error;
    }
    set_vec_member_struct_name(checker, node, elem_type);
    Type* option_type = lookup_or_instantiate_option_type(checker, elem_type);
    return type_func(NULL, 0, option_type, 0);
}

static int is_vec_builtin_mutating_method(const char* member_name) {
    return strcmp(member_name, "push") == 0 || strcmp(member_name, "insert") == 0 ||
           strcmp(member_name, "remove") == 0 || strcmp(member_name, "swap_remove") == 0 ||
           strcmp(member_name, "clear") == 0 || strcmp(member_name, "reserve") == 0 ||
           strcmp(member_name, "shrink_to_fit") == 0 || strcmp(member_name, "sort") == 0;
}

static Type* check_member_vec_mutating_method(Checker* checker, Node* node, Type* elem_type) {
    const char* member_name = node->as.member.name;
    if (check_mutating_member_on_const(checker, node, member_name)) {
        return type_error;
    }
    if (strcmp(member_name, "sort") == 0 && !type_supports_vec_sort(elem_type)) {
        check_error(checker, node->line, node->column,
                    "Vec.sort requires orderable primitive element type, got '%s'",
                    type_name(elem_type));
        return type_error;
    }

    set_vec_member_struct_name(checker, node, elem_type);

    if (strcmp(member_name, "push") == 0) {
        Type** params = xmalloc(sizeof(Type*));
        params[0]     = elem_type;
        return type_func(params, 1, type_void, 0);
    }
    if (strcmp(member_name, "insert") == 0) {
        Type** params = xmalloc(2 * sizeof(Type*));
        params[0]     = type_int64;
        params[1]     = elem_type;
        return type_func(params, 2, type_void, 0);
    }
    if (strcmp(member_name, "remove") == 0 || strcmp(member_name, "swap_remove") == 0) {
        Type** params = xmalloc(sizeof(Type*));
        params[0]     = type_int64;
        return type_func(params, 1, elem_type, 0);
    }
    if (strcmp(member_name, "reserve") == 0) {
        Type** params = xmalloc(sizeof(Type*));
        params[0]     = type_int64;
        return type_func(params, 1, type_void, 0);
    }
    return type_func(NULL, 0, type_void, 0);
}

static Type* check_member_vec_user_method(Checker* checker, Node* node, Type* elem_type) {
    const char* member_name = node->as.member.name;
    char        lookup_mangled[256];
    snprintf(lookup_mangled, sizeof(lookup_mangled), "Vec_%s", type_mangle_name(elem_type));
    VecInstance* inst = lookup_vec_instance_pub(checker, lookup_mangled);
    if (!inst) {
        return NULL;
    }

    ensure_vec_user_methods(checker, inst);
    for (int i = 0; i < inst->method_count; i++) {
        if (strcmp(inst->method_names[i], member_name) == 0) {
            if (!inst->method_is_const[i] &&
                check_mutating_member_on_const(checker, node, member_name)) {
                return type_error;
            }
            set_vec_member_struct_name(checker, node, elem_type);
            return inst->method_types[i];
        }
    }
    return NULL;
}

static Type* check_member_vec(Checker* checker, Node* node, Type* object) {
    const char* member_name = node->as.member.name;
    sem_info_set_member_is_ref(checker->sem, node, 1); // Vec is a pointer (RC-managed)
    Type* elem_type = object->as.vec.elem;

    if (strcmp(member_name, "count") == 0) {
        return type_int64;
    }
    if (strcmp(member_name, "capacity") == 0) {
        return type_int64;
    }
    if (strcmp(member_name, "data") == 0) {
        check_error(checker, node->line, node->column, "Vec 'data' field is private; use indexing");
        return type_error;
    }
    // Non-mutating methods: contains, first, last
    if (strcmp(member_name, "contains") == 0) {
        return check_member_vec_contains(checker, node, elem_type);
    }

    // first/last return Option<T>
    if (strcmp(member_name, "first") == 0 || strcmp(member_name, "last") == 0) {
        return check_member_vec_option_method(checker, node, elem_type, 0);
    }

    // pop is mutating and returns Option<T>
    if (strcmp(member_name, "pop") == 0) {
        return check_member_vec_option_method(checker, node, elem_type, 1);
    }

    // Mutating methods: push, insert, remove, swap_remove, clear, reserve, shrink_to_fit, sort
    if (is_vec_builtin_mutating_method(member_name)) {
        return check_member_vec_mutating_method(checker, node, elem_type);
    }

    // Look up user-defined methods on VecInstance
    Type* user_method = check_member_vec_user_method(checker, node, elem_type);
    if (user_method) {
        return user_method;
    }

    check_error(checker, node->line, node->column, "Vec has no member '%s'", member_name);
    return type_error;
}

// Check string member access (length, contains, starts_with, ends_with, index_of, split)
// Returns NULL if member_name is not a built-in string method (falls through to primitive methods)
static Type* check_member_string(Checker* checker, Node* node) {
    const char* member_name = node->as.member.name;
    sem_info_set_member_is_ref(checker->sem, node, 0);
    if (strcmp(member_name, "length") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        return type_func(NULL, 0, type_int64, 0);
    }
    if (strcmp(member_name, "contains") == 0 || strcmp(member_name, "starts_with") == 0 ||
        strcmp(member_name, "ends_with") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_string;
        return type_func(params, 1, type_bool, 0);
    }
    if (strcmp(member_name, "index_of") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_string;
        return type_func(params, 1, type_int64, 0);
    }
    if (strcmp(member_name, "split") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_string;
        Type* ret     = ensure_vec_type(checker, type_string);
        return type_func(params, 1, ret, 0);
    }
    if (strcmp(member_name, "trim") == 0 || strcmp(member_name, "trim_start") == 0 ||
        strcmp(member_name, "trim_end") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        return type_func(NULL, 0, type_string, 0);
    }
    if (strcmp(member_name, "strip_prefix") == 0 || strcmp(member_name, "strip_suffix") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_string;
        return type_func(params, 1, type_string, 0);
    }
    if (strcmp(member_name, "pad_left") == 0 || strcmp(member_name, "pad_right") == 0) {
        sem_info_set_member_struct_name(checker->sem, node, "__String");
        Type** params = xmalloc(2 * sizeof(Type*));
        params[0]     = type_int64;
        params[1]     = type_char;
        return type_func(params, 2, type_string, 0);
    }
    return NULL; // Not a built-in string method; fall through to primitive methods
}

// Check methods on primitive types (from trait impls, e.g., Hashable)
static Type* check_member_primitive(Checker* checker, Node* node, Type* object) {
    const char* prim_name   = type_name(object);
    const char* member_name = node->as.member.name;
    for (int i = 0; i < checker->traits.primitive_method_count; i++) {
        if (strcmp(checker->traits.primitive_methods[i].type_name, prim_name) == 0 &&
            strcmp(checker->traits.primitive_methods[i].method_name, member_name) == 0) {
            if (!checker->traits.primitive_methods[i].is_const) {
                const char* const_name = get_const_binding_name(checker, node->as.member.object);
                if (const_name) {
                    check_error(checker, node->line, node->column,
                                "Cannot call mutating method '%s' on const '%s'", member_name,
                                const_name);
                    return type_error;
                }
            }
            sem_info_set_member_is_ref(checker->sem, node, 0);
            sem_info_set_member_struct_name(checker->sem, node, prim_name);
            return checker->traits.primitive_methods[i].method_type;
        }
    }
    if (object->kind == TYPE_STRING) {
        check_error(checker, node->line, node->column, "String has no member '%s'", member_name);
    } else {
        check_error(checker, node->line, node->column,
                    "Member access requires struct type, got '%s'", type_name(object));
    }
    return type_error;
}

// Check struct field and method access
static Type* check_member_struct(Checker* checker, Node* node, Type* object) {
    sem_info_set_member_is_ref(checker->sem, node, 1);
    const char* member_name = node->as.member.name;

    // Find field first
    for (int i = 0; i < object->as.struc.field_count; i++) {
        if (strcmp(object->as.struc.field_names[i], member_name) == 0) {
            // Check private field access
            if (object->as.struc.field_is_private && object->as.struc.field_is_private[i]) {
                if (!checker->current_method_receiver ||
                    strcmp(object->as.struc.base_name, checker->current_method_receiver) != 0) {
                    check_error(checker, node->line, node->column, "Field '%s' is private",
                                member_name);
                    return type_error;
                }
            }
            int object_is_const = 0;
            if (node->as.member.object->type == NODE_IDENT) {
                Symbol* sym     = checker_lookup(checker, node->as.member.object->as.ident.name);
                object_is_const = (sym && sym->is_const);
            } else if (node->as.member.object->type == NODE_MEMBER) {
                object_is_const = sem_info_get_member_is_const_access(
                    checker->sem, node->as.member.object,
                    node->as.member.object->as.member.is_const_access);
            }
            sem_info_set_member_struct_name(checker->sem, node, NULL);
            sem_info_set_member_is_const_access(
                checker->sem, node, object->as.struc.field_is_const[i] || object_is_const);
            return object->as.struc.field_types[i];
        }
    }

    // If not a field, check for method
    for (int i = 0; i < object->as.struc.method_count; i++) {
        if (strcmp(object->as.struc.method_names[i], member_name) == 0) {
            if (!object->as.struc.method_is_const[i]) {
                const char* const_name = get_const_binding_name(checker, node->as.member.object);
                if (const_name) {
                    check_error(checker, node->line, node->column,
                                "Cannot call mutating method '%s' on const '%s'", member_name,
                                const_name);
                    return type_error;
                }
            }
            sem_info_set_member_struct_name(checker->sem, node, object->as.struc.name);
            return object->as.struc.method_types[i];
        }
    }

    check_error(checker, node->line, node->column, "Struct '%s' has no field or method '%s'",
                object->as.struc.name, member_name);
    return type_error;
}

// Check StringBuilder member access (append, append_char, append_line, len, capacity, clear,
// to_string)
static Type* check_member_stringbuilder(Checker* checker, Node* node) {
    const char* member_name = node->as.member.name;
    sem_info_set_member_is_ref(checker->sem, node, 1); // StringBuilder is RC-managed pointer
    sem_info_set_member_struct_name(checker->sem, node, "__StringBuilder");

    // Mutating methods
    if (strcmp(member_name, "append") == 0 || strcmp(member_name, "append_line") == 0) {
        const char* const_name = get_const_binding_name(checker, node->as.member.object);
        if (const_name) {
            check_error(checker, node->line, node->column,
                        "Cannot call mutating method '%s' on const '%s'", member_name, const_name);
            return type_error;
        }
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_string;
        return type_func(params, 1, type_void, 0);
    }
    if (strcmp(member_name, "append_char") == 0) {
        const char* const_name = get_const_binding_name(checker, node->as.member.object);
        if (const_name) {
            check_error(checker, node->line, node->column,
                        "Cannot call mutating method '%s' on const '%s'", member_name, const_name);
            return type_error;
        }
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_char;
        return type_func(params, 1, type_void, 0);
    }
    if (strcmp(member_name, "clear") == 0) {
        const char* const_name = get_const_binding_name(checker, node->as.member.object);
        if (const_name) {
            check_error(checker, node->line, node->column,
                        "Cannot call mutating method '%s' on const '%s'", member_name, const_name);
            return type_error;
        }
        return type_func(NULL, 0, type_void, 0);
    }

    // Non-mutating methods
    if (strcmp(member_name, "len") == 0 || strcmp(member_name, "capacity") == 0) {
        return type_func(NULL, 0, type_int64, 0);
    }
    if (strcmp(member_name, "to_string") == 0) {
        return type_func(NULL, 0, type_string, 0);
    }

    check_error(checker, node->line, node->column, "StringBuilder has no member '%s'", member_name);
    return type_error;
}

// Check for Type.method unbound method reference (e.g., Point.move)
static Type* check_member_type_ref(Checker* checker, Node* node, Type* type_val) {
    const char* member_name = node->as.member.name;
    Type*       method_type = NULL;
    const char* sname       = NULL;

    if (type_val->kind == TYPE_STRUCT) {
        for (int i = 0; i < type_val->as.struc.method_count; i++) {
            if (strcmp(type_val->as.struc.method_names[i], member_name) == 0) {
                method_type = type_val->as.struc.method_types[i];
                sname       = type_val->as.struc.name;
                break;
            }
        }
    } else if (type_val->kind == TYPE_ENUM) {
        for (int i = 0; i < type_val->as.enm.method_count; i++) {
            if (strcmp(type_val->as.enm.method_names[i], member_name) == 0) {
                method_type = type_val->as.enm.method_types[i];
                sname       = type_val->as.enm.name;
                break;
            }
        }
    }

    if (!method_type || method_type->kind != TYPE_FUNC) {
        check_error(checker, node->line, node->column, "Type '%s' has no method '%s'",
                    type_name(type_val), member_name);
        return type_error;
    }

    // Build new TYPE_FUNC with receiver prepended as first parameter
    int    old_count = method_type->as.func.param_count;
    int    new_count = old_count + 1;
    Type** params    = xmalloc(new_count * sizeof(Type*));
    params[0]        = type_val;
    for (int i = 0; i < old_count; i++) {
        params[i + 1] = method_type->as.func.param_types[i];
    }

    sem_info_set_member_struct_name(checker->sem, node, sname);
    node->as.member.is_method_ref = 1;

    Type* result = type_func(params, new_count, method_type->as.func.return_type, 0);
    node->as.member.resolved_func_type = result;
    return result;
}

// Type-check a member access: dispatch to type-specific helpers
static Type* check_member_expr(Checker* checker, Node* node) {
    // Module access must use :: not . (e.g., std::print, not std.print)
    if (node->as.member.object->type == NODE_IDENT) {
        const char* name = node->as.member.object->as.ident.name;
        if (is_imported_module(checker, name)) {
            check_error(checker, node->line, node->column,
                        "Use '::' for module access (e.g., %s::%s)", name, node->as.member.name);
            return type_error;
        }
    }

    // Check for Type.method unbound method reference (e.g., Point.move)
    if (node->as.member.object->type == NODE_IDENT) {
        const char* name = node->as.member.object->as.ident.name;
        Symbol*     sym  = checker_lookup(checker, name);
        if (sym && sym->kind == SYM_TYPE) {
            return check_member_type_ref(checker, node, sym->type);
        }
    }

    Type* object = check_expression(checker, node->as.member.object);
    if (object->kind == TYPE_ERROR)
        return type_error;

    switch (object->kind) {
    case TYPE_SPAN:
        return check_member_span(checker, node);
    case TYPE_ENUM:
        return check_member_enum(checker, node, object);
    case TYPE_VEC:
        return check_member_vec(checker, node, object);
    case TYPE_STRINGBUILDER:
        return check_member_stringbuilder(checker, node);
    case TYPE_STRING: {
        Type* result = check_member_string(checker, node);
        if (result)
            return result;
        // Fall through to primitive methods for trait impls (e.g., Hashable)
        return check_member_primitive(checker, node, object);
    }
    case TYPE_STRUCT:
        return check_member_struct(checker, node, object);
    default:
        return check_member_primitive(checker, node, object);
    }
}

// Check if a node is a valid assignment target (identifier, member, or index)
static int is_lvalue(Node* node) {
    if (!node)
        return 0;

    switch (node->type) {
    case NODE_IDENT:
    case NODE_MEMBER:
    case NODE_INDEX:
        return 1;
    default:
        return 0;
    }
}

// Check that assigning to an identifier target does not mutate a const binding.
static int check_ident_assignment_mutability(Checker* checker, Node* target, int line, int column) {
    Symbol* sym = checker_lookup(checker, target->as.ident.name);
    if (sym && sym->is_const) {
        check_error(checker, line, column, "Cannot assign to const '%s'", target->as.ident.name);
        return 0;
    }
    return 1;
}

// Check that assigning through a member target does not mutate const state.
static int check_member_assignment_mutability(Checker* checker, Node* target, int line,
                                              int column) {
    Node* obj = target->as.member.object;
    if (obj->type == NODE_IDENT) {
        Symbol* sym = checker_lookup(checker, obj->as.ident.name);
        if (sym && sym->is_const) {
            check_error(checker, line, column, "Cannot modify field '%s' through const '%s'",
                        target->as.member.name, obj->as.ident.name);
            return 0;
        }
    }
    if (sem_info_get_member_is_const_access(checker->sem, target,
                                            target->as.member.is_const_access)) {
        check_error(checker, line, column, "Cannot assign to const field '%s'",
                    target->as.member.name);
        return 0;
    }
    return 1;
}

// Check that assigning through an index target does not write to a const binding.
static int check_index_assignment_mutability(Checker* checker, Node* target, int line, int column) {
    const char* const_name = get_const_binding_name(checker, target->as.index.object);
    if (const_name) {
        check_error(checker, line, column, "Cannot write to index of const '%s'", const_name);
        return 0;
    }
    return 1;
}

// Validate assignment target shape and mutability rules for lvalue writes.
static int check_assignment_target_writable(Checker* checker, Node* assign_node) {
    Node* target = assign_node->as.assign.target;
    if (!is_lvalue(target)) {
        check_error(checker, assign_node->line, assign_node->column, "Invalid assignment target");
        return 0;
    }

    switch (target->type) {
    case NODE_IDENT:
        return check_ident_assignment_mutability(checker, target, assign_node->line,
                                                 assign_node->column);
    case NODE_MEMBER:
        return check_member_assignment_mutability(checker, target, assign_node->line,
                                                  assign_node->column);
    case NODE_INDEX:
        return check_index_assignment_mutability(checker, target, assign_node->line,
                                                 assign_node->column);
    default:
        return 1;
    }
}

// Disallow writes to .tag on data enums, which is a read-only synthesized field.
static int check_assignment_enum_tag_writable(Checker* checker, Node* assign_node) {
    Node* target = assign_node->as.assign.target;
    if (target->type != NODE_MEMBER || strcmp(target->as.member.name, "tag") != 0) {
        return 1;
    }

    Type* obj_type = check_expression(checker, target->as.member.object);
    if (obj_type && obj_type->kind == TYPE_ENUM && obj_type->as.enm.has_data) {
        check_error(checker, assign_node->line, assign_node->column,
                    "Enum tag is read-only; cannot assign to '%s.tag'", obj_type->as.enm.name);
        return 0;
    }
    return 1;
}

// Disallow assignment to captured value types inside lambdas.
// Value types are copied into the closure environment, so assignment modifies the copy, not the
// original. Returns 0 (and emits an error) if this is a forbidden captured value-type assignment.
static int check_captured_value_assignment(Checker* checker, Node* assign_node, Type* target) {
    if (checker->lambda_depth <= 0)
        return 1;

    Node* lhs = assign_node->as.assign.target;
    if (lhs->type != NODE_IDENT)
        return 1;

    int boundaries = count_captured_boundaries(checker, lhs->as.ident.name);
    if (boundaries <= 0)
        return 1;

    // RC-managed types and func types are heap-allocated pointers — assignment is fine
    if (type_is_rc_managed(target) || (target && target->kind == TYPE_FUNC))
        return 1;

    check_error(checker, assign_node->line, assign_node->column,
                "Cannot assign to captured value type '%s' in lambda; value types are copied into "
                "the closure — use 'var ^%s' to create a shared mutable Box",
                lhs->as.ident.name, lhs->as.ident.name);
    return 0;
}

// Validate that compound assignment uses numeric operands on both sides.
static int check_compound_assignment_operands(Checker* checker, Node* assign_node, Type* target,
                                              Type* value) {
    if (assign_node->as.assign.op == TOK_EQ) {
        return 1;
    }
    if (!is_numeric_comparable_type(target) || !is_numeric_comparable_type(value)) {
        check_error(checker, assign_node->line, assign_node->column,
                    "Invalid operands for compound assignment");
        return 0;
    }
    return 1;
}

// Type-check an assignment: validate lvalue, const, enum tag, and type compatibility
static Type* check_assign_expr(Checker* checker, Node* node) {
    Type* target = check_expression(checker, node->as.assign.target);

    if (node->as.assign.value && node->as.assign.value->type == NODE_STRUCT_INIT) {
        check_error(checker, node->line, node->column,
                    "Struct initializers are only allowed in variable declarations");
        return type_error;
    }

    Type* value = check_expression(checker, node->as.assign.value);

    if (target->kind == TYPE_ERROR || value->kind == TYPE_ERROR)
        return type_error;

    if (!check_assignment_target_writable(checker, node)) {
        return type_error;
    }

    if (!check_assignment_enum_tag_writable(checker, node)) {
        return type_error;
    }

    if (!check_captured_value_assignment(checker, node, target)) {
        return type_error;
    }

    // Auto-deref Box<T> target: `b = 20` becomes `b->value = 20`
    if (target->kind == TYPE_BOX) {
        target = auto_deref_box(node->as.assign.target, target);
    }

    // Auto-deref Box<T> value when assigning to T
    if (value->kind == TYPE_BOX && type_assignable(target, value->as.box.elem)) {
        value = auto_deref_box(node->as.assign.value, value);
    }

    if (!check_compound_assignment_operands(checker, node, target, value)) {
        return type_error;
    }

    if (!type_assignable(target, value)) {
        check_error_type(checker, node->line, node->column, "Assignment", target, value);
        return type_error;
    }

    return target;
}

// --- Expression case helpers ---

static int find_template_enum_variant(Node* enum_decl, const char* value_name) {
    for (int i = 0; i < enum_decl->as.enum_decl.values.count; i++) {
        Node* variant = enum_decl->as.enum_decl.values.nodes[i];
        if (strcmp(variant->as.enum_variant.name, value_name) == 0) {
            return i;
        }
    }
    return -1;
}

static int all_type_args_inferred(Type** inferred, int type_param_count) {
    for (int i = 0; i < type_param_count; i++) {
        if (!inferred[i]) {
            return 0;
        }
    }
    return 1;
}

static void fill_missing_inferred_from_enum_target_hint(Checker* checker, const char* enum_name,
                                                        Type** inferred, int type_param_count) {
    if (!checker->enum_target_hint || checker->enum_target_hint->kind != TYPE_ENUM) {
        return;
    }

    // The target hint is an already-instantiated generic enum.
    for (int gi = 0; gi < checker->generics.instance_count; gi++) {
        GenericInstance* inst = &checker->generics.instances[gi];
        if (inst->type == checker->enum_target_hint && strcmp(inst->base_name, enum_name) == 0) {
            // Use instance type args for any missing inferred params.
            for (int p = 0; p < type_param_count && p < inst->type_arg_count; p++) {
                if (!inferred[p]) {
                    inferred[p] = inst->type_args[p];
                }
            }
            return;
        }
    }
}

static Type* resolve_generic_enum_value_type(Checker* checker, Node* node, const char* enum_name) {
    const char* value_name = node->as.enum_value.value_name;
    int         arg_count  = node->as.enum_value.args.count;
    GenericDef* def        = lookup_generic_def(checker, enum_name);
    Type*       enum_type  = type_error;

    if (!def || def->decl->type != NODE_ENUM_DECL) {
        check_error(checker, node->line, node->column, "Unknown enum '%s'", enum_name);
        return type_error;
    }

    Node* template_decl = def->decl;
    int   variant_idx   = find_template_enum_variant(template_decl, value_name);
    if (variant_idx < 0) {
        check_error(checker, node->line, node->column, "'%s' is not a value of enum '%s'",
                    value_name, enum_name);
        return type_error;
    }

    Node* variant    = template_decl->as.enum_decl.values.nodes[variant_idx];
    int   type_count = variant->as.enum_variant.types.count;
    if (arg_count != type_count) {
        check_error(checker, node->line, node->column,
                    "Enum variant '%s::%s' expects %d argument(s), got %d", enum_name, value_name,
                    type_count, arg_count);
        return type_error;
    }

    Type** inferred  = xcalloc(def->type_param_count, sizeof(Type*));
    Type** arg_types = NULL;
    if (arg_count > 0) {
        arg_types = xmalloc(arg_count * sizeof(Type*));
        for (int i = 0; i < arg_count; i++) {
            arg_types[i] = check_expression(checker, node->as.enum_value.args.nodes[i]);
        }
    }

    // Infer from constructor argument types first.
    for (int i = 0; i < arg_count; i++) {
        Node* vtype_node = variant->as.enum_variant.types.nodes[i];
        if (vtype_node->type != NODE_IDENT) {
            continue;
        }
        for (int p = 0; p < def->type_param_count; p++) {
            if (strcmp(vtype_node->as.ident.name, def->type_params[p]) == 0) {
                if (!inferred[p]) {
                    inferred[p] = arg_types[i];
                }
                break;
            }
        }
    }

    if (!all_type_args_inferred(inferred, def->type_param_count)) {
        fill_missing_inferred_from_enum_target_hint(checker, enum_name, inferred,
                                                    def->type_param_count);
    }

    if (!all_type_args_inferred(inferred, def->type_param_count)) {
        check_error(checker, node->line, node->column,
                    "Cannot infer type parameters for generic enum '%s'; add explicit type "
                    "annotation",
                    enum_name);
        free(inferred);
        free(arg_types);
        return type_error;
    }

    char* mangled = type_mangle_generic(enum_name, inferred, def->type_param_count);

    GenericInstance* existing = lookup_generic_instance(checker, mangled);
    if (existing) {
        enum_type = existing->type;
        free(mangled);
    } else {
        // instantiate_generic_enum takes ownership of args_copy.
        Type** args_copy = xmalloc(def->type_param_count * sizeof(Type*));
        for (int p = 0; p < def->type_param_count; p++) {
            args_copy[p] = inferred[p];
        }
        enum_type =
            instantiate_generic_enum(checker, def, mangled, args_copy, def->type_param_count);
    }

    free(inferred);
    free(arg_types);
    return enum_type;
}

// Type-check module::func or module::func(args) parsed as NODE_ENUM_VALUE
static Type* check_module_call_expr(Checker* checker, Node* node) {
    const char* module_name = node->as.enum_value.enum_name;
    const char* func_name   = node->as.enum_value.value_name;
    int         arg_count   = node->as.enum_value.args.count;

    node->as.enum_value.is_module_call = 1;

    // Built-in: std::format(string, ...) -> string
    if (strcmp(module_name, "std") == 0 && strcmp(func_name, "format") == 0) {
        if (arg_count < 1) {
            check_error(checker, node->line, node->column,
                        "std::format requires at least 1 argument");
            return type_error;
        }
        Type* fmt_type = check_expression(checker, node->as.enum_value.args.nodes[0]);
        if (fmt_type->kind != TYPE_ERROR && !type_assignable(type_string, fmt_type)) {
            check_error(checker, node->line, node->column,
                        "std::format first argument must be string, got '%s'", type_name(fmt_type));
            return type_error;
        }
        for (int i = 1; i < arg_count; i++) {
            check_expression(checker, node->as.enum_value.args.nodes[i]);
        }
        node->is_owned_temp   = 1;
        node->owned_temp_type = type_string;
        return type_string;
    }

    // Look up the function in the module
    Symbol* sym = checker_lookup_in_module(checker, module_name, func_name);
    if (!sym) {
        check_error(checker, node->line, node->column, "Module '%s' has no public symbol '%s'",
                    module_name, func_name);
        return type_error;
    }

    Type* func_type = sym->type;

    // If no parens, this is a function/const reference (e.g., std::println as a value)
    if (!node->as.enum_value.has_parens) {
        return func_type;
    }

    // Validate it's callable
    if (func_type->kind != TYPE_FUNC) {
        check_error(checker, node->line, node->column, "Module symbol '%s::%s' is not callable",
                    module_name, func_name);
        return type_error;
    }

    // Check argument count
    int expected   = func_type->as.func.param_count;
    int is_varargs = func_type->as.func.is_varargs;
    if (is_varargs ? (arg_count < expected) : (arg_count != expected)) {
        check_error(checker, node->line, node->column, "'%s::%s' expects %s%d argument(s), got %d",
                    module_name, func_name, is_varargs ? "at least " : "", expected, arg_count);
        return type_error;
    }

    // Type-check each argument
    for (int i = 0; i < arg_count; i++) {
        Type* arg_type = check_expression(checker, node->as.enum_value.args.nodes[i]);
        if (i < expected && arg_type->kind != TYPE_ERROR) {
            Type* param_type = func_type->as.func.param_types[i];
            if (!type_assignable(param_type, arg_type)) {
                check_error(checker, node->as.enum_value.args.nodes[i]->line,
                            node->as.enum_value.args.nodes[i]->column,
                            "'%s::%s' argument %d: expected '%s', got '%s'", module_name, func_name,
                            i + 1, type_name(param_type), type_name(arg_type));
            }
        }
    }

    Type* ret = func_type->as.func.return_type;
    if (type_is_rc_managed(ret)) {
        node->is_owned_temp   = 1;
        node->owned_temp_type = ret;
    }
    return ret;
}

// Type-check an enum variant constructor, including generic enum type inference
static Type* check_enum_value_expr(Checker* checker, Node* node) {
    const char* enum_name  = node->as.enum_value.enum_name;
    const char* value_name = node->as.enum_value.value_name;

    // Check if this is module::func access (e.g., std::println)
    if (is_imported_module(checker, enum_name)) {
        return check_module_call_expr(checker, node);
    }

    Symbol* sym       = checker_lookup(checker, enum_name);
    Type*   enum_type = NULL;

    if (sym && sym->kind == SYM_TYPE && sym->type->kind == TYPE_ENUM) {
        // Non-generic enum or already-instantiated generic enum.
        enum_type = sym->type;
    } else {
        enum_type = resolve_generic_enum_value_type(checker, node, enum_name);
        if (enum_type == type_error) {
            return type_error;
        }
    }

    sem_info_set_enum_value_resolved_name(checker->sem, node, enum_type->as.enm.name);

    // Check that the value exists in the (possibly instantiated) enum.
    int variant_idx = type_enum_variant_index(enum_type, value_name);
    if (variant_idx < 0) {
        check_error(checker, node->line, node->column, "'%s' is not a value of enum '%s'",
                    value_name, enum_type->as.enm.name);
        return type_error;
    }

    // Set is_data_enum flag and resolved type for codegen.
    sem_info_set_enum_value_is_data_enum(checker->sem, node, enum_type->as.enm.has_data);
    node->as.enum_value.resolved_type = enum_type;

    // Validate constructor args for data enums.
    int expected_args = enum_type->as.enm.variant_type_counts[variant_idx];
    int actual_args   = node->as.enum_value.args.count;

    if (actual_args != expected_args) {
        check_error(checker, node->line, node->column,
                    "Enum variant '%s::%s' expects %d argument(s), got %d", enum_type->as.enm.name,
                    value_name, expected_args, actual_args);
        return type_error;
    }

    // Type-check each constructor arg.
    for (int i = 0; i < actual_args; i++) {
        Type* arg_type = check_expression(checker, node->as.enum_value.args.nodes[i]);
        Type* expected = enum_type->as.enm.variant_types[variant_idx][i];
        if (arg_type->kind != TYPE_ERROR && !type_assignable(expected, arg_type)) {
            check_error(checker, node->as.enum_value.args.nodes[i]->line,
                        node->as.enum_value.args.nodes[i]->column,
                        "Enum variant '%s::%s' argument %d: expected '%s', got '%s'",
                        enum_type->as.enm.name, value_name, i + 1, type_name(expected),
                        type_name(arg_type));
        }
    }

    return enum_type;
}

static int ident_matches(Node* node, const char* name, int len) {
    return node->type == NODE_IDENT && node->as.ident.length == len &&
           strncmp(node->as.ident.name, name, len) == 0;
}

static int try_check_sameref_builtin(Checker* checker, Node* node, Type** out_type) {
    if (!ident_matches(node->as.call.func, "sameref", 7)) {
        return 0;
    }
    if (node->as.call.args.count != 2) {
        check_error(checker, node->line, node->column, "sameref() requires exactly 2 arguments");
        *out_type = type_error;
        return 1;
    }

    Type* a = check_expression(checker, node->as.call.args.nodes[0]);
    Type* b = check_expression(checker, node->as.call.args.nodes[1]);
    if (a->kind != TYPE_STRUCT) {
        check_error(checker, node->line, node->column,
                    "sameref() requires struct arguments, got '%s'", type_name(a));
        *out_type = type_error;
        return 1;
    }
    if (!type_equals(a, b)) {
        check_error(checker, node->line, node->column, "sameref() arguments must be the same type");
        *out_type = type_error;
        return 1;
    }

    *out_type = type_bool;
    return 1;
}

static int try_check_assert_builtin(Checker* checker, Node* node, Type** out_type) {
    if (!ident_matches(node->as.call.func, "assert", 6) || checker_lookup(checker, "assert")) {
        return 0;
    }
    if (node->as.call.args.count != 1) {
        check_error(checker, node->line, node->column, "assert() requires exactly 1 argument");
        *out_type = type_error;
        return 1;
    }

    Type* arg_type = check_expression(checker, node->as.call.args.nodes[0]);
    if (arg_type->kind != TYPE_ERROR && !type_equals(arg_type, type_bool)) {
        check_error(checker, node->line, node->column, "assert() argument must be bool, got '%s'",
                    type_name(arg_type));
        *out_type = type_error;
        return 1;
    }

    *out_type = type_void;
    return 1;
}

static int type_satisfies_trait_bound(Checker* checker, Type* type, const char* trait_name) {
    const char* type_name_str = NULL;
    if (type->kind == TYPE_STRUCT) {
        type_name_str = type->as.struc.name;
    } else {
        type_name_str = type_name(type);
    }
    if (!type_name_str) {
        return 0;
    }

    for (int i = 0; i < checker->traits.impl_count; i++) {
        if (strcmp(checker->traits.impls[i].trait_name, trait_name) == 0 &&
            strcmp(checker->traits.impls[i].type_name, type_name_str) == 0) {
            return 1;
        }
    }
    return 0;
}

// Check if a node is a lambda with at least one untyped parameter
static int has_untyped_lambda_params(Node* node) {
    if (node->type != NODE_LAMBDA)
        return 0;
    for (int i = 0; i < node->as.lambda.params.count; i++) {
        if (!node->as.lambda.params.nodes[i]->as.param.type)
            return 1;
    }
    return 0;
}

// Build an expected TYPE_FUNC from a generic parameter type node using partially-inferred types.
// Only resolves param types whose type nodes are simple ident type params with known inference.
// Returns NULL if any param type can't be resolved.
static Type* build_expected_func_type(Checker* checker, Node* param_type_node, GenericFuncDef* gdef,
                                      Type** inferred) {
    if (!param_type_node || param_type_node->type != NODE_FUNC_TYPE)
        return NULL;

    NodeList* ft_params   = &param_type_node->as.func_type.param_types;
    int       n           = ft_params->count;
    Type**    param_types = xmalloc(n * sizeof(Type*));

    for (int i = 0; i < n; i++) {
        Node* pt       = ft_params->nodes[i];
        param_types[i] = NULL;

        if (pt->type == NODE_IDENT) {
            // Check if it's a generic type param with known inference
            for (int j = 0; j < gdef->type_param_count; j++) {
                if (strcmp(pt->as.ident.name, gdef->type_params[j]) == 0) {
                    param_types[i] = inferred[j]; // NULL if not yet inferred
                    break;
                }
            }
            // If not a type param, try to resolve as concrete type
            if (!param_types[i]) {
                param_types[i] = resolve_type(checker, pt);
                if (param_types[i]->kind == TYPE_ERROR)
                    param_types[i] = NULL;
            }
        }

        if (!param_types[i]) {
            free(param_types);
            return NULL;
        }
    }

    return type_func(param_types, n, NULL, 0);
}

static Type* try_check_generic_free_function_call(Checker* checker, Node* node) {
    Node* callee = node->as.call.func;
    if (callee->type != NODE_IDENT) {
        return NULL;
    }

    GenericFuncDef* gdef = lookup_generic_func_def(checker, callee->as.ident.name);
    if (!gdef) {
        return NULL;
    }

    func_decl_node* fdn = &gdef->decl->as.func_decl;
    if (node->as.call.args.count != fdn->params.count) {
        check_error(checker, node->line, node->column,
                    "Generic function '%s' expects %d arguments, got %d", gdef->name,
                    fdn->params.count, node->as.call.args.count);
        return type_error;
    }

    int    arg_count = node->as.call.args.count;
    Type** arg_types = xcalloc(arg_count, sizeof(Type*));
    Type** inferred  = xcalloc(gdef->type_param_count, sizeof(Type*));
    Type*  result    = type_error;

    // Pass 1: Check non-lambda-with-untyped-params arguments
    for (int i = 0; i < arg_count; i++) {
        if (has_untyped_lambda_params(node->as.call.args.nodes[i])) {
            continue;
        }

        arg_types[i] = check_expression(checker, node->as.call.args.nodes[i]);
        if (arg_types[i]->kind == TYPE_ERROR) {
            goto cleanup;
        }
        infer_type_param(checker, gdef, fdn->params.nodes[i]->as.param.type, arg_types[i],
                         inferred);
    }

    // Pass 2: Check lambda args with expected types built from partial inference
    for (int i = 0; i < arg_count; i++) {
        if (arg_types[i]) {
            continue;
        }

        Type* old_expected = checker->expected_func_type;
        Type* expected =
            build_expected_func_type(checker, fdn->params.nodes[i]->as.param.type, gdef, inferred);
        if (expected) {
            checker->expected_func_type = expected;
        }

        arg_types[i]                = check_expression(checker, node->as.call.args.nodes[i]);
        checker->expected_func_type = old_expected;
        if (arg_types[i]->kind == TYPE_ERROR) {
            goto cleanup;
        }

        infer_type_param(checker, gdef, fdn->params.nodes[i]->as.param.type, arg_types[i],
                         inferred);
    }

    for (int i = 0; i < gdef->type_param_count; i++) {
        if (!inferred[i]) {
            check_error(checker, node->line, node->column,
                        "Cannot infer type parameter '%s' for generic function '%s'",
                        gdef->type_params[i], gdef->name);
            goto cleanup;
        }
    }

    for (int i = 0; i < gdef->type_param_count; i++) {
        const char* bound = gdef->type_param_bounds[i];
        if (!bound) {
            continue;
        }
        if (!type_satisfies_trait_bound(checker, inferred[i], bound)) {
            check_error(checker, node->line, node->column,
                        "Type '%s' does not implement trait '%s' required by '%s'",
                        type_name(inferred[i]), bound, gdef->name);
            goto cleanup;
        }
    }

    GenericFuncInstance* inst = instantiate_generic_func(
        checker, gdef, inferred, gdef->type_param_count, node->line, node->column);
    if (!inst) {
        goto cleanup;
    }

    node->as.call.resolved_name = xstrdup(inst->mangled_name);
    Type* ret                   = inst->func_type->as.func.return_type;
    if (type_is_rc_managed(ret)) {
        node->is_owned_temp   = 1;
        node->owned_temp_type = ret;
    }
    result = ret;

cleanup:
    free(arg_types);
    free(inferred);
    return result;
}

// Determine the base receiver type name and type arguments for method-level generics.
// For Vec<i64>: base_name = "Vec", recv_args = [i64], count = 1
// For Box<i64> (generic struct): base_name = "Box", recv_args = [i64], count = 1
// Returns NULL base_name if the type cannot be a generic method receiver.
static const char* get_receiver_info(Checker* checker, Type* recv_type, Type*** out_args,
                                     int* out_count) {
    *out_args  = NULL;
    *out_count = 0;

    if (recv_type->kind == TYPE_VEC) {
        *out_args      = xmalloc(sizeof(Type*));
        (*out_args)[0] = recv_type->as.vec.elem;
        *out_count     = 1;
        return "Vec";
    }

    if (recv_type->kind == TYPE_STRUCT || recv_type->kind == TYPE_ENUM) {
        const char* concrete_name =
            (recv_type->kind == TYPE_STRUCT) ? recv_type->as.struc.name : recv_type->as.enm.name;

        // Look up GenericInstance to recover base name + concrete type args.
        for (int i = 0; i < checker->generics.instance_count; i++) {
            GenericInstance* inst = &checker->generics.instances[i];
            if (strcmp(inst->mangled_name, concrete_name) == 0) {
                *out_args = xmalloc(inst->type_arg_count * sizeof(Type*));
                for (int j = 0; j < inst->type_arg_count; j++) {
                    (*out_args)[j] = inst->type_args[j];
                }
                *out_count = inst->type_arg_count;
                return inst->base_name;
            }
        }

        // Non-generic struct/enum — use type name as-is (no type args)
        return concrete_name;
    }

    return NULL;
}

// Try to resolve a method-level generic call: items.map(|x| x * 2)
// Returns the return type if this is a method-level generic call, or NULL if not.
static int should_skip_generic_method_call(Checker* checker, Node* callee) {
    // Skip module-qualified calls (e.g., std.print) and type references (e.g., Point.move).
    // These are not method calls on a receiver object.
    if (callee->as.member.object->type != NODE_IDENT) {
        return 0;
    }

    const char* name = callee->as.member.object->as.ident.name;
    if (is_imported_module(checker, name)) {
        return 1;
    }

    Symbol* sym = checker_lookup(checker, name);
    return sym && sym->kind == SYM_TYPE;
}

static int check_generic_method_arg_count(Checker* checker, Node* node, func_decl_node* fdn,
                                          const char* method_name) {
    if (node->as.call.args.count == fdn->params.count) {
        return 1;
    }

    check_error(checker, node->line, node->column, "Method '%s' expects %d arguments, got %d",
                method_name, fdn->params.count, node->as.call.args.count);
    return 0;
}

static void infer_method_type_params_from_receiver(Checker* checker, GenericFuncDef* gdef,
                                                   func_decl_node* fdn, Type** recv_args,
                                                   int recv_arg_count, Type** inferred) {
    int recv_pattern_count = fdn->receiver_type_args.count;
    for (int i = 0; i < recv_arg_count && i < recv_pattern_count; i++) {
        infer_type_param(checker, gdef, fdn->receiver_type_args.nodes[i], recv_args[i], inferred);
    }
}

static Type** check_generic_method_args(Checker* checker, Node* node, GenericFuncDef* gdef,
                                        func_decl_node* fdn, Type** inferred) {
    int    arg_count = node->as.call.args.count;
    Type** arg_types = xmalloc(arg_count * sizeof(Type*));
    int    has_error = 0;

    for (int i = 0; i < arg_count; i++) {
        // Build expected type for lambda args from partially-inferred params.
        Type* old_expected    = checker->expected_func_type;
        Node* param_type_node = fdn->params.nodes[i]->as.param.type;
        Type* expected        = build_expected_func_type(checker, param_type_node, gdef, inferred);
        if (expected) {
            checker->expected_func_type = expected;
        }

        arg_types[i] = check_expression(checker, node->as.call.args.nodes[i]);

        checker->expected_func_type = old_expected;

        if (arg_types[i]->kind == TYPE_ERROR) {
            has_error = 1;
        }
    }

    if (has_error) {
        free(arg_types);
        return NULL;
    }
    return arg_types;
}

static void infer_method_type_params_from_args(Checker* checker, GenericFuncDef* gdef,
                                               func_decl_node* fdn, Type** arg_types, int arg_count,
                                               Type** inferred) {
    for (int i = 0; i < arg_count; i++) {
        Node* param_type_node = fdn->params.nodes[i]->as.param.type;
        infer_type_param(checker, gdef, param_type_node, arg_types[i], inferred);
    }
}

static int ensure_generic_method_inference_complete(Checker* checker, Node* node,
                                                    GenericFuncDef* gdef, const char* base_name,
                                                    const char* method_name, Type** inferred) {
    for (int i = 0; i < gdef->type_param_count; i++) {
        if (!inferred[i]) {
            check_error(checker, node->line, node->column,
                        "Cannot infer type parameter '%s' for generic method '%s.%s'",
                        gdef->type_params[i], base_name, method_name);
            return 0;
        }
    }
    return 1;
}

static int check_generic_method_trait_bounds(Checker* checker, Node* node, GenericFuncDef* gdef,
                                             const char* base_name, const char* method_name,
                                             Type** inferred) {
    for (int i = 0; i < gdef->type_param_count; i++) {
        const char* bound = gdef->type_param_bounds[i];
        if (!bound) {
            continue;
        }
        if (!type_satisfies_trait_bound(checker, inferred[i], bound)) {
            check_error(checker, node->line, node->column,
                        "Type '%s' does not implement trait '%s' required by '%s.%s'",
                        type_name(inferred[i]), bound, base_name, method_name);
            return 0;
        }
    }
    return 1;
}

static Type* try_check_generic_method_call(Checker* checker, Node* node) {
    Node* callee = node->as.call.func;
    if (callee->type != NODE_MEMBER) {
        return NULL;
    }

    if (should_skip_generic_method_call(checker, callee)) {
        return NULL;
    }

    // Type-check the receiver object
    Type* recv_type = check_expression(checker, callee->as.member.object);
    if (recv_type->kind == TYPE_ERROR) {
        return NULL;
    }

    Type**      recv_args      = NULL;
    int         recv_arg_count = 0;
    const char* base_name      = get_receiver_info(checker, recv_type, &recv_args, &recv_arg_count);
    const char* method_name    = callee->as.member.name;
    if (!base_name) {
        return NULL;
    }

    // Look up method-level generic definition
    GenericFuncDef* gdef = lookup_generic_method_func_def(checker, base_name, method_name);
    if (!gdef) {
        free(recv_args);
        return NULL;
    }

    func_decl_node* fdn = &gdef->decl->as.func_decl;

    // Validate argument count (method params, not counting self)
    if (!check_generic_method_arg_count(checker, node, fdn, method_name)) {
        free(recv_args);
        return type_error;
    }

    // Pre-fill inference array from receiver pattern BEFORE checking args
    Type** inferred = xcalloc(gdef->type_param_count, sizeof(Type*));
    infer_method_type_params_from_receiver(checker, gdef, fdn, recv_args, recv_arg_count, inferred);
    free(recv_args);

    Type** arg_types = check_generic_method_args(checker, node, gdef, fdn, inferred);
    if (!arg_types) {
        free(inferred);
        return type_error;
    }

    // Infer remaining type params from arguments
    int arg_count = node->as.call.args.count;
    infer_method_type_params_from_args(checker, gdef, fdn, arg_types, arg_count, inferred);

    // Check all type params were inferred
    if (!ensure_generic_method_inference_complete(checker, node, gdef, base_name, method_name,
                                                  inferred)) {
        free(arg_types);
        free(inferred);
        return type_error;
    }

    // Check trait bounds
    if (!check_generic_method_trait_bounds(checker, node, gdef, base_name, method_name, inferred)) {
        free(arg_types);
        free(inferred);
        return type_error;
    }

    // Instantiate
    GenericFuncInstance* inst = instantiate_generic_method_func(
        checker, gdef, inferred, gdef->type_param_count, recv_type, node->line, node->column);
    free(arg_types);
    free(inferred);

    if (!inst) {
        return type_error;
    }

    // Set resolved_name for codegen dispatch
    node->as.call.resolved_name = xstrdup(inst->mangled_name);
    Type* ret                   = inst->func_type->as.func.return_type;
    if (type_is_rc_managed(ret)) {
        node->is_owned_temp   = 1;
        node->owned_temp_type = ret;
    }
    return ret;
}

static int check_call_arg_count(Checker* checker, Node* node, Type* func_type) {
    if (func_type->as.func.is_varargs) {
        if (node->as.call.args.count < func_type->as.func.param_count) {
            check_error(checker, node->line, node->column, "Expected at least %d arguments, got %d",
                        func_type->as.func.param_count, node->as.call.args.count);
            return 0;
        }
        return 1;
    }

    if (node->as.call.args.count != func_type->as.func.param_count) {
        check_error(checker, node->line, node->column, "Expected %d arguments, got %d",
                    func_type->as.func.param_count, node->as.call.args.count);
        return 0;
    }
    return 1;
}

static void check_call_named_args(Checker* checker, Node* node, Type* func_type) {
    for (int i = 0; i < func_type->as.func.param_count; i++) {
        Type* param_type = func_type->as.func.param_types[i];

        // Set expected type for lambda inference
        Type* old_expected = checker->expected_func_type;
        if (param_type->kind == TYPE_FUNC) {
            checker->expected_func_type = param_type;
        }

        Type* arg_type = check_expression(checker, node->as.call.args.nodes[i]);

        checker->expected_func_type = old_expected;

        // Auto-deref Box<T> argument when parameter expects T
        if (!type_assignable(param_type, arg_type) && arg_type->kind == TYPE_BOX &&
            type_assignable(param_type, arg_type->as.box.elem)) {
            arg_type = auto_deref_box(node->as.call.args.nodes[i], arg_type);
        }

        if (!type_assignable(param_type, arg_type)) {
            char ctx[32];
            snprintf(ctx, sizeof(ctx), "Argument %d", i + 1);
            check_error_type(checker, node->as.call.args.nodes[i]->line,
                             node->as.call.args.nodes[i]->column, ctx, param_type, arg_type);
        }
    }
}

static void check_call_variadic_tail_args(Checker* checker, Node* node, Type* func_type) {
    for (int i = func_type->as.func.param_count; i < node->as.call.args.count; i++) {
        check_expression(checker, node->as.call.args.nodes[i]);
    }
}

// Type-check a function call: validate callee, argument count, and argument types
static Type* check_call_expr(Checker* checker, Node* node) {
    Type* builtin_type = NULL;
    if (try_check_sameref_builtin(checker, node, &builtin_type) ||
        try_check_assert_builtin(checker, node, &builtin_type)) {
        return builtin_type;
    }

    Type* generic_call_type = try_check_generic_free_function_call(checker, node);
    if (generic_call_type) {
        return generic_call_type;
    }

    Type* generic_method_type = try_check_generic_method_call(checker, node);
    if (generic_method_type) {
        return generic_method_type;
    }

    Type* func_type = check_expression(checker, node->as.call.func);
    if (func_type->kind == TYPE_ERROR) {
        return type_error;
    }
    if (func_type->kind != TYPE_FUNC) {
        check_error_cannot(checker, node->line, node->column, "call", func_type);
        return type_error;
    }
    if (!check_call_arg_count(checker, node, func_type)) {
        return type_error;
    }

    // Determine if this is an indirect call (through a closure value)
    Node* callee = node->as.call.func;
    if (callee->type == NODE_IDENT) {
        Symbol* sym = checker_lookup(checker, callee->as.ident.name);
        if (sym && sym->kind == SYM_VAR) {
            node->as.call.is_indirect_call   = 1;
            node->as.call.resolved_func_type = func_type;
        } else {
            // Direct function call — clear func ref flag since this is a call, not a value ref
            callee->as.ident.resolved_func_type = NULL;
        }
    } else {
        // Non-ident callee (lambda, member field access, etc.) — always indirect
        node->as.call.is_indirect_call   = 1;
        node->as.call.resolved_func_type = func_type;
    }

    check_call_named_args(checker, node, func_type);
    check_call_variadic_tail_args(checker, node, func_type);
    Type* ret = func_type->as.func.return_type;
    if (type_is_rc_managed(ret)) {
        node->is_owned_temp   = 1;
        node->owned_temp_type = ret;
    }
    return ret;
}

static void set_new_expr_result(Node* node, Type* resolved) {
    node->as.new_expr.resolved_type = resolved;
    node->is_owned_temp             = 1;
    node->owned_temp_type           = resolved;
}

static Type* lookup_struct_init_method(Type* struct_type) {
    for (int i = 0; i < struct_type->as.struc.method_count; i++) {
        if (strcmp(struct_type->as.struc.method_names[i], "init") == 0) {
            return struct_type->as.struc.method_types[i];
        }
    }
    return NULL;
}

static Type* check_new_constructor_call_expr(Checker* checker, Node* node, Type* resolved) {
    // Box<T>(expr) constructor form
    if (resolved->kind == TYPE_BOX) {
        if (node->as.new_expr.args.count != 1) {
            check_error(checker, node->line, node->column,
                        "Box constructor requires exactly 1 argument");
            return type_error;
        }
        Type* arg_type = check_expression(checker, node->as.new_expr.args.nodes[0]);
        if (arg_type->kind == TYPE_ERROR) {
            return type_error;
        }
        if (!type_assignable(resolved->as.box.elem, arg_type)) {
            check_error_type(checker, node->line, node->column, "Box value", resolved->as.box.elem,
                             arg_type);
            return type_error;
        }
        set_new_expr_result(node, resolved);
        return resolved;
    }

    if (resolved->kind != TYPE_STRUCT) {
        check_error(checker, node->line, node->column,
                    "'new' constructor call requires a struct type");
        return type_error;
    }
    if (!resolved->as.struc.has_init) {
        check_error(checker, node->line, node->column, "Type '%s' does not have an init method",
                    resolved->as.struc.name);
        return type_error;
    }

    Type* init_func_type = lookup_struct_init_method(resolved);
    if (!init_func_type) {
        check_error(checker, node->line, node->column, "Type '%s' does not have an init method",
                    resolved->as.struc.name);
        return type_error;
    }

    int expected_params = init_func_type->as.func.param_count;
    int actual_args     = node->as.new_expr.args.count;
    if (actual_args != expected_params) {
        check_error(checker, node->line, node->column, "init expects %d argument(s), got %d",
                    expected_params, actual_args);
        return type_error;
    }

    for (int i = 0; i < actual_args; i++) {
        Type* arg_type = check_expression(checker, node->as.new_expr.args.nodes[i]);
        if (arg_type->kind == TYPE_ERROR) {
            continue;
        }

        Type* param_type = init_func_type->as.func.param_types[i];
        if (!type_assignable(param_type, arg_type)) {
            check_error(checker, node->as.new_expr.args.nodes[i]->line,
                        node->as.new_expr.args.nodes[i]->column,
                        "init argument %d: expected '%s', got '%s'", i + 1, type_name(param_type),
                        type_name(arg_type));
        }
    }

    set_new_expr_result(node, resolved);
    return resolved;
}

static Type* check_new_vec_literal_expr(Checker* checker, Node* node, Type* resolved) {
    Type* elem_type = resolved->as.vec.elem;
    Node* init      = node->as.new_expr.init;

    // Check each element expression against the element type.
    for (int i = 0; i < init->as.struct_init.fields.count; i++) {
        Node* field = init->as.struct_init.fields.nodes[i];
        if (field->type != NODE_FIELD_INIT) {
            continue;
        }

        // Set enum_target_hint for generic enum inference (e.g., new
        // Vec<Option<i64>>{Option::None})
        Type* old_hint = checker->enum_target_hint;
        if (elem_type->kind == TYPE_ENUM) {
            checker->enum_target_hint = elem_type;
        }
        Type* val_type            = check_expression(checker, field->as.field_init.value);
        checker->enum_target_hint = old_hint;
        if (val_type->kind != TYPE_ERROR && !type_assignable(elem_type, val_type)) {
            check_error_type(checker, field->line, field->column, "Vec element", elem_type,
                             val_type);
        }
    }

    set_new_expr_result(node, resolved);
    return resolved;
}

static Type* check_new_stringbuilder_literal_expr(Checker* checker, Node* node, Type* resolved) {
    Node* init = node->as.new_expr.init;
    if (init->as.struct_init.fields.count > 0) {
        check_error(checker, node->line, node->column,
                    "StringBuilder does not accept field initializers");
        return type_error;
    }

    set_new_expr_result(node, resolved);
    return resolved;
}

static Type* check_new_struct_literal_expr(Checker* checker, Node* node, Type* resolved) {
    if (resolved->kind != TYPE_STRUCT) {
        check_error(checker, node->line, node->column, "'new' requires a struct type, got '%s'",
                    type_name(resolved));
        return type_error;
    }

    // Error if struct has init but user uses literal form.
    if (resolved->as.struc.has_init) {
        check_error(checker, node->line, node->column,
                    "Type '%s' has an init method; use 'new %s(args)' instead of 'new %s { ... }'",
                    resolved->as.struc.name, resolved->as.struc.name, resolved->as.struc.name);
        return type_error;
    }

    Type* init_type = check_struct_init(checker, node->as.new_expr.init, resolved);
    if (init_type == type_error) {
        return type_error;
    }

    set_new_expr_result(node, resolved);
    return resolved;
}

// Type-check a `new` expression: resolve type, validate struct init, Vec elements, or init call
static Type* check_new_expr(Checker* checker, Node* node) {
    Type* resolved = resolve_type(checker, node->as.new_expr.type_node);
    if (resolved == type_error) {
        return type_error;
    }

    // Init-call form: new Type(args)
    if (node->as.new_expr.init == NULL) {
        return check_new_constructor_call_expr(checker, node, resolved);
    }

    // Box<T> literal form: new Box<T>{value: expr}
    if (resolved->kind == TYPE_BOX) {
        Node* init = node->as.new_expr.init;
        if (init->as.struct_init.fields.count != 1) {
            check_error(checker, node->line, node->column,
                        "Box initializer requires exactly 1 field 'value'");
            return type_error;
        }
        Node* field = init->as.struct_init.fields.nodes[0];
        if (!field || field->type != NODE_FIELD_INIT ||
            strcmp(field->as.field_init.name, "value") != 0) {
            check_error(checker, node->line, node->column,
                        "Box initializer requires field named 'value'");
            return type_error;
        }
        Type* val_type = check_expression(checker, field->as.field_init.value);
        if (val_type->kind == TYPE_ERROR) {
            return type_error;
        }
        if (!type_assignable(resolved->as.box.elem, val_type)) {
            check_error_type(checker, field->line, field->column, "Box value",
                             resolved->as.box.elem, val_type);
            return type_error;
        }
        set_new_expr_result(node, resolved);
        return resolved;
    }

    // Struct literal form below: new Type { fields }
    if (resolved->kind == TYPE_VEC) {
        return check_new_vec_literal_expr(checker, node, resolved);
    }
    if (resolved->kind == TYPE_STRINGBUILDER) {
        return check_new_stringbuilder_literal_expr(checker, node, resolved);
    }
    return check_new_struct_literal_expr(checker, node, resolved);
}

// Type-check an array literal: infer element type from first element, validate the rest
static Type* check_array_lit_expr(Checker* checker, Node* node) {
    int count = node->as.array_lit.elements.count;
    if (count == 0) {
        check_error(checker, node->line, node->column,
                    "Empty array literal requires type annotation");
        return type_error;
    }

    // Check first element to get the element type
    Type* elem_type = check_expression(checker, node->as.array_lit.elements.nodes[0]);
    if (elem_type->kind == TYPE_ERROR) {
        return type_error;
    }

    // Check remaining elements have compatible types
    for (int i = 1; i < count; i++) {
        Type* t = check_expression(checker, node->as.array_lit.elements.nodes[i]);
        if (t->kind == TYPE_ERROR) {
            return type_error;
        }
        if (!type_assignable(elem_type, t)) {
            check_error(checker, node->as.array_lit.elements.nodes[i]->line,
                        node->as.array_lit.elements.nodes[i]->column,
                        "Array element type mismatch: expected '%s', got '%s'",
                        type_name(elem_type), type_name(t));
            return type_error;
        }
    }

    // Store resolved element type for codegen
    node->as.array_lit.resolved_type = elem_type;
    return type_array(elem_type, count);
}

// =============================================================================
// Expression checking — small helpers
// =============================================================================

// Type-check a cast expression: validate that source and target types are compatible
static Type* check_cast_expr(Checker* checker, Node* node) {
    Type* expr_type = check_expression(checker, node->as.cast_expr.expr);
    if (expr_type->kind == TYPE_ERROR)
        return type_error;
    Type* target = resolve_type(checker, node->as.cast_expr.type_node);
    if (target->kind == TYPE_ERROR)
        return type_error;
    node->as.cast_expr.resolved_type = target;

    // Allow: identity cast
    if (expr_type == target)
        return target;
    // Allow: char -> integer
    if (expr_type->kind == TYPE_CHAR && type_is_integer(target))
        return target;
    // Allow: integer -> char
    if (type_is_integer(expr_type) && target->kind == TYPE_CHAR)
        return target;
    // Allow: integer -> integer (widening/narrowing)
    if (type_is_integer(expr_type) && type_is_integer(target))
        return target;
    // Allow: simple enum -> integer (for explicit/ordinal value access)
    if (expr_type->kind == TYPE_ENUM && !expr_type->as.enm.has_data && type_is_integer(target))
        return target;
    // Allow: struct reference -> voidptr (opaque pointer for interop/hash use cases)
    if (expr_type->kind == TYPE_STRUCT && target->kind == TYPE_VOIDPTR)
        return target;
    // Allow: voidptr -> u64 (for pointer hashing/interop handles)
    if (expr_type->kind == TYPE_VOIDPTR && target->kind == TYPE_UINT64)
        return target;

    check_error(checker, node->line, node->column, "Cannot cast '%s' to '%s'", type_name(expr_type),
                type_name(target));
    return type_error;
}

// Type-check a tuple literal: resolve each element type and return a tuple type
static Type* check_tuple_lit_expr(Checker* checker, Node* node) {
    int    count = node->as.tuple_lit.elements.count;
    Type** elems = xmalloc(count * sizeof(Type*));
    for (int i = 0; i < count; i++) {
        elems[i] = check_expression(checker, node->as.tuple_lit.elements.nodes[i]);
    }
    Type* tuple_type                 = type_tuple(elems, count);
    node->as.tuple_lit.resolved_type = tuple_type;
    return tuple_type;
}

// Type-check a string interpolation: validate that all parts are formattable types
static Type* check_string_interp_expr(Checker* checker, Node* node) {
    int count                         = node->as.string_interp.parts.count;
    node->as.string_interp.part_types = xcalloc(count, sizeof(Type*));
    node->as.string_interp.part_count = count;
    for (int i = 0; i < count; i++) {
        Node* part = node->as.string_interp.parts.nodes[i];
        if (part->type == NODE_STRING_LIT) {
            node->as.string_interp.part_types[i] = type_string;
        } else {
            Type* t = check_expression(checker, part);
            if (t->kind == TYPE_ERROR) {
                node->as.string_interp.part_types[i] = type_error;
                continue;
            }
            if (!type_is_integer(t) && t->kind != TYPE_F32 && t->kind != TYPE_F64 &&
                t->kind != TYPE_BOOL && t->kind != TYPE_STRING && t->kind != TYPE_CHAR) {
                check_error(checker, part->line, part->column,
                            "String interpolation does not support type '%s'", type_name(t));
                node->as.string_interp.part_types[i] = type_error;
                continue;
            }
            node->as.string_interp.part_types[i] = t;
        }
    }
    // String interp with expressions produces an owned string (via __std_format)
    int has_expr = 0;
    for (int i = 0; i < count; i++) {
        if (node->as.string_interp.parts.nodes[i]->type != NODE_STRING_LIT) {
            has_expr = 1;
            break;
        }
    }
    if (has_expr) {
        node->is_owned_temp   = 1;
        node->owned_temp_type = type_string;
    }
    return type_string;
}

// =============================================================================
// Try expression checking (? operator)
// =============================================================================

static int classify_try_enum(Type* enum_type, int* is_option, int* success_idx, int* err_idx) {
    int ok_idx   = type_enum_variant_index(enum_type, "Ok");
    int err      = type_enum_variant_index(enum_type, "Err");
    int some_idx = type_enum_variant_index(enum_type, "Some");
    int none_idx = type_enum_variant_index(enum_type, "None");

    int is_result = (ok_idx >= 0 && err >= 0);
    int option    = (some_idx >= 0 && none_idx >= 0);
    if (is_result) {
        option = 0;
    }

    if (!is_result && !option) {
        return 0;
    }

    *is_option   = option;
    *success_idx = is_result ? ok_idx : some_idx;
    *err_idx     = is_result ? err : -1;
    return 1;
}

static int check_try_return_type_compatibility(Checker* checker, Node* node, Type* expr_type,
                                               Type* ret_type, int is_option, int expr_err_idx) {
    if (ret_type->kind != TYPE_ENUM || !ret_type->as.enm.has_data) {
        check_error(checker, node->line, node->column,
                    "'?' used in function returning '%s', but must return a Result or Option type",
                    type_name(ret_type));
        return 0;
    }

    if (is_option) {
        if (type_enum_variant_index(ret_type, "None") < 0) {
            check_error(checker, node->line, node->column,
                        "'?' on Option requires function return type to have 'None' variant, "
                        "got '%s'",
                        type_name(ret_type));
            return 0;
        }
        return 1;
    }

    int ret_err_idx = type_enum_variant_index(ret_type, "Err");
    if (ret_err_idx < 0) {
        check_error(checker, node->line, node->column,
                    "'?' on Result requires function return type to have 'Err' variant, got '%s'",
                    type_name(ret_type));
        return 0;
    }

    if (expr_type->as.enm.variant_type_counts[expr_err_idx] != 1 ||
        ret_type->as.enm.variant_type_counts[ret_err_idx] != 1) {
        check_error(checker, node->line, node->column,
                    "'?' requires 'Err' variant to have exactly one payload field");
        return 0;
    }

    Type* expr_err_type = expr_type->as.enm.variant_types[expr_err_idx][0];
    Type* ret_err_type  = ret_type->as.enm.variant_types[ret_err_idx][0];
    if (!type_assignable(ret_err_type, expr_err_type)) {
        check_error(checker, node->line, node->column,
                    "'?' error type mismatch: expression has Err(%s) but function returns Err(%s)",
                    type_name(expr_err_type), type_name(ret_err_type));
        return 0;
    }
    return 1;
}

static void set_try_expr_info(Node* node, Type* expr_type, Type* ret_type, Type* unwrapped_type,
                              int is_option) {
    node->as.try_expr.resolved_type  = expr_type;
    node->as.try_expr.unwrapped_type = unwrapped_type;
    node->as.try_expr.is_option      = is_option ? 1 : 0;
    node->as.try_expr.enum_name      = xstrdup(expr_type->as.enm.name);
    node->as.try_expr.ret_enum_name  = xstrdup(ret_type->as.enm.name);
}

// Type-check a try expression (expr?): validates Result/Option pattern,
// extracts unwrapped type, and checks return type compatibility
static Type* check_try_expr(Checker* checker, Node* node) {
    Type* expr_type = check_expression(checker, node->as.try_expr.expr);
    if (expr_type->kind == TYPE_ERROR)
        return type_error;

    // Must be a data enum
    if (expr_type->kind != TYPE_ENUM || !expr_type->as.enm.has_data) {
        check_error(checker, node->line, node->column,
                    "'?' operator requires a Result or Option type, got '%s'",
                    type_name(expr_type));
        return type_error;
    }

    // Must be inside a function
    if (!checker->current_func_return) {
        check_error(checker, node->line, node->column,
                    "'?' operator can only be used inside a function");
        return type_error;
    }

    int is_option   = 0;
    int success_idx = -1;
    int err_idx     = -1;
    if (!classify_try_enum(expr_type, &is_option, &success_idx, &err_idx)) {
        check_error(checker, node->line, node->column,
                    "'?' operator requires a Result (Ok/Err) or Option (Some/None) enum, got '%s'",
                    type_name(expr_type));
        return type_error;
    }

    // Extract unwrapped type T from Ok(T) or Some(T)
    if (expr_type->as.enm.variant_type_counts[success_idx] != 1) {
        check_error(checker, node->line, node->column,
                    "'?' requires '%s' variant to have exactly one payload field",
                    is_option ? "Some" : "Ok");
        return type_error;
    }
    Type* unwrapped_type = expr_type->as.enm.variant_types[success_idx][0];

    // Validate function return type compatibility
    Type* ret_type = checker->current_func_return;
    if (!check_try_return_type_compatibility(checker, node, expr_type, ret_type, is_option,
                                             err_idx)) {
        return type_error;
    }

    set_try_expr_info(node, expr_type, ret_type, unwrapped_type, is_option);
    return unwrapped_type;
}

// =============================================================================
// Lambda expression checking
// =============================================================================

// Check if a variable name is captured across a lambda boundary
// Count how many lambda boundaries are crossed to reach the variable's definition.
// Returns 0 if no boundary is crossed (variable is local). Returns the count of
// lambda boundaries crossed, or 0 if the variable isn't found.
static int count_captured_boundaries(Checker* checker, const char* name) {
    int    crossed = 0;
    Scope* scope   = checker->scope;
    while (scope) {
        unsigned hash = 5381;
        for (const char* p = name; *p; p++)
            hash = ((hash << 5) + hash) + (unsigned char)*p;
        int idx = hash % scope->size;
        for (Symbol* sym = scope->symbols[idx]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0 && sym->kind == SYM_VAR) {
                return crossed;
            }
        }
        if (scope->is_lambda_boundary)
            crossed++;
        scope = scope->parent;
    }
    return 0;
}

// Add a capture to a lambda node (deduplicated by name).
static void add_capture(Node* lambda, const char* name, Type* type) {
    for (int i = 0; i < lambda->as.lambda.captures.count; i++) {
        if (strcmp(lambda->as.lambda.captures.names[i], name) == 0)
            return;
    }
    int cnt = lambda->as.lambda.captures.count;
    int cap = lambda->as.lambda.captures.capacity;
    if (cnt >= cap) {
        cap = cap ? cap * 2 : 4;
        lambda->as.lambda.captures.names =
            xrealloc(lambda->as.lambda.captures.names, cap * sizeof(char*));
        lambda->as.lambda.captures.types =
            xrealloc(lambda->as.lambda.captures.types, cap * sizeof(Type*));
        lambda->as.lambda.captures.is_rc =
            xrealloc(lambda->as.lambda.captures.is_rc, cap * sizeof(int));
        lambda->as.lambda.captures.capacity = cap;
    }
    lambda->as.lambda.captures.names[cnt] = xstrdup(name);
    lambda->as.lambda.captures.types[cnt] = type;
    lambda->as.lambda.captures.is_rc[cnt] =
        type_is_rc_managed(type) || (type && type->kind == TYPE_FUNC);
    lambda->as.lambda.captures.count = cnt + 1;
}

// Collect captures for a variable that crosses lambda boundaries.
// Adds the variable to each intermediate lambda's capture list.
static void collect_captures(Checker* checker, const char* name, Type* type, int boundary_count) {
    int start = checker->lambda_stack_count - boundary_count;
    if (start < 0)
        start = 0;
    for (int i = start; i < checker->lambda_stack_count; i++) {
        add_capture(checker->lambda_stack[i], name, type);
    }
}

// Resolve lambda parameter types from annotations or the expected function type.
static int resolve_lambda_param_types(Checker* checker, Node* lambda, Type*** out_param_types,
                                      int* out_param_count) {
    NodeList* params      = &lambda->as.lambda.params;
    int       param_count = params->count;
    Type**    param_types = NULL;
    if (param_count > 0) {
        param_types = xmalloc(param_count * sizeof(Type*));
    }

    for (int i = 0; i < param_count; i++) {
        Node* p = params->nodes[i];
        if (p->as.param.type) {
            param_types[i] = resolve_type(checker, p->as.param.type);
        } else if (checker->expected_func_type && checker->expected_func_type->kind == TYPE_FUNC &&
                   i < checker->expected_func_type->as.func.param_count &&
                   checker->expected_func_type->as.func.param_types[i]) {
            param_types[i] = checker->expected_func_type->as.func.param_types[i];
        } else {
            check_error(checker, p->line, p->column, "Cannot infer type for lambda parameter '%s'",
                        p->as.param.name);
            free(param_types);
            return 0;
        }

        if (param_types[i]->kind == TYPE_ERROR) {
            free(param_types);
            return 0;
        }
    }

    *out_param_types = param_types;
    *out_param_count = param_count;
    return 1;
}

// Resolve an explicitly declared lambda return type, if present.
static Type* resolve_lambda_return_type(Checker* checker, Node* lambda) {
    if (!lambda->as.lambda.return_type) {
        return NULL;
    }
    return resolve_type(checker, lambda->as.lambda.return_type);
}

// Define lambda parameters in the current scope.
static void define_lambda_params_in_scope(Checker* checker, Node* lambda, Type** param_types,
                                          int param_count) {
    NodeList* params = &lambda->as.lambda.params;
    for (int i = 0; i < param_count; i++) {
        Node* p = params->nodes[i];
        checker_define(checker, p->as.param.name, SYM_VAR, param_types[i], p->as.param.is_const, 0,
                       NULL);
    }
}

// Type-check a lambda body and return the effective lambda return type.
static Type* check_lambda_body(Checker* checker, Node* lambda, Type* declared_return_type) {
    Type* return_type = declared_return_type;

    if (lambda->as.lambda.is_expr_body) {
        checker->current_func_return = return_type ? return_type : type_void;
        Type* body_type              = check_expression(checker, lambda->as.lambda.body);
        if (!return_type) {
            return_type = body_type;
        } else if (!type_assignable(return_type, body_type)) {
            check_error(checker, lambda->as.lambda.body->line, lambda->as.lambda.body->column,
                        "Lambda body type '%s' doesn't match return type '%s'",
                        type_name(body_type), type_name(return_type));
        }
        return return_type;
    }

    if (!return_type) {
        return_type = type_void;
    }
    checker->current_func_return = return_type;
    Node* body                   = lambda->as.lambda.body;
    if (body && body->type == NODE_BLOCK) {
        for (int i = 0; i < body->as.block.stmts.count; i++) {
            check_statement(checker, body->as.block.stmts.nodes[i]);
        }
    }
    return return_type;
}

static Type* check_lambda_expr(Checker* checker, Node* node) {
    // Assign unique ID
    node->as.lambda.lambda_id = checker->lambda_next_id++;

    Type** param_types = NULL;
    int    param_count = 0;
    if (!resolve_lambda_param_types(checker, node, &param_types, &param_count)) {
        return type_error;
    }

    // Resolve explicit return type
    Type* return_type = resolve_lambda_return_type(checker, node);
    if (return_type && return_type->kind == TYPE_ERROR) {
        free(param_types);
        return type_error;
    }

    // Push lambda scope
    checker_push_scope(checker);
    checker->scope->is_lambda_boundary = 1;

    // Save and set function return context
    Type* old_return = checker->current_func_return;
    checker->lambda_depth++;

    // Push lambda onto capture tracking stack
    VEC_GROW(checker->lambda_stack, checker->lambda_stack_count, checker->lambda_stack_capacity);
    checker->lambda_stack[checker->lambda_stack_count++] = node;

    define_lambda_params_in_scope(checker, node, param_types, param_count);
    return_type = check_lambda_body(checker, node, return_type);

    // Restore state
    checker->lambda_stack_count--;
    checker->lambda_depth--;
    checker->current_func_return = old_return;
    checker_pop_scope(checker);

    // Build TYPE_FUNC
    Type* func_type               = type_func(param_types, param_count, return_type, 0);
    node->as.lambda.resolved_type = func_type;
    // param_types ownership transferred to type_func

    // Lambdas with captures allocate an env via __rc_alloc — mark as owned temp
    // so the hoist pattern will dec the env after the call site
    if (node->as.lambda.captures.count > 0) {
        node->is_owned_temp = 1;
    }

    return func_type;
}

// =============================================================================
// Expression checking
// =============================================================================

// Return the built-in type for literal expression nodes, or NULL otherwise.
static Type* get_literal_expr_type(int node_type) {
    switch (node_type) {
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
    default:
        return NULL;
    }
}

// Collect lambda captures when an identifier resolves to a variable across boundaries.
static void maybe_collect_ident_capture(Checker* checker, Node* node, Symbol* sym) {
    if (checker->lambda_depth <= 0 || !sym || sym->kind != SYM_VAR) {
        return;
    }

    int boundaries = count_captured_boundaries(checker, node->as.ident.name);
    if (boundaries > 0) {
        collect_captures(checker, node->as.ident.name, sym->type, boundaries);
    }
}

// Type-check an identifier expression and record resolved function type metadata.
static Type* check_ident_expr(Checker* checker, Node* node) {
    Symbol* sym = checker_lookup(checker, node->as.ident.name);
    if (!sym) {
        check_error(checker, node->line, node->column, "Undefined identifier '%s'",
                    node->as.ident.name);
        return type_error;
    }

    maybe_collect_ident_capture(checker, node, sym);

    if (sym->kind == SYM_FUNC && sym->type->kind == TYPE_FUNC) {
        node->as.ident.resolved_func_type = sym->type;
    }
    return sym->type;
}

// Report use of a struct initializer where a contextual struct type is required.
static Type* check_contextless_struct_init_expr(Checker* checker, Node* node) {
    check_error(checker, node->line, node->column,
                "Struct initializer requires a contextual struct type");
    return type_error;
}

// Validate an `is` expression used as a general expression (not in if/while condition).
// Returns TYPE_BOOL. Bindings are NOT allowed in this context.
static Type* check_is_expr(Checker* checker, Node* node) {
    Type* expr_type = check_expression(checker, node->as.is_expr.expr);
    if (expr_type->kind == TYPE_ERROR)
        return type_error;

    if (expr_type->kind != TYPE_ENUM) {
        check_error(checker, node->line, node->column,
                    "'is' pattern requires an enum type, got '%s'", type_name(expr_type));
        return type_error;
    }

    node->as.is_expr.resolved_type = expr_type;

    // Validate variant exists
    const char* variant_name = node->as.is_expr.variant_name;
    int         variant_idx  = type_enum_variant_index(expr_type, variant_name);
    if (variant_idx < 0) {
        check_error(checker, node->line, node->column, "'%s' is not a variant of enum '%s'",
                    variant_name, expr_type->as.enm.name);
        return type_error;
    }

    // Validate qualified enum name if present (allow generic base name)
    if (node->as.is_expr.enum_name) {
        const char* user_name   = node->as.is_expr.enum_name;
        const char* actual_name = expr_type->as.enm.name;
        int         user_len    = node->as.is_expr.enum_name_length;
        if (strcmp(user_name, actual_name) != 0 &&
            !(strncmp(user_name, actual_name, user_len) == 0 && actual_name[user_len] == '_')) {
            check_error(checker, node->line, node->column,
                        "Enum name '%s' does not match expression type '%s'", user_name,
                        actual_name);
            return type_error;
        }
    }

    // Bindings are only allowed in if/while condition context
    if (node->as.is_expr.binding_count > 0) {
        check_error(checker, node->line, node->column,
                    "'is' pattern with bindings only allowed in if/while conditions");
        return type_error;
    }

    return type_bool;
}

// Dispatch non-literal expression nodes to their dedicated type-checking routines.
static Type* check_non_literal_expression(Checker* checker, Node* node) {
    switch (node->type) {
    case NODE_IDENT:
        return check_ident_expr(checker, node);

    case NODE_ENUM_VALUE:
        return check_enum_value_expr(checker, node);

    case NODE_BINARY:
        return check_binary_expr(checker, node);

    case NODE_UNARY:
        return check_unary_expr(checker, node);

    case NODE_CALL:
        return check_call_expr(checker, node);

    case NODE_INDEX:
        return check_index_expr(checker, node);

    case NODE_SLICE:
        return check_slice_expr(checker, node);

    case NODE_MEMBER:
        return check_member_expr(checker, node);

    case NODE_ASSIGN:
        return check_assign_expr(checker, node);

    case NODE_NEW_EXPR:
        return check_new_expr(checker, node);

    case NODE_CAST:
        return check_cast_expr(checker, node);

    case NODE_TRY_EXPR:
        return check_try_expr(checker, node);

    case NODE_MATCH:
        return check_match_expr(checker, node);

    case NODE_STRUCT_INIT:
        return check_contextless_struct_init_expr(checker, node);

    case NODE_TUPLE_LIT:
        return check_tuple_lit_expr(checker, node);

    case NODE_ARRAY_LIT:
        return check_array_lit_expr(checker, node);

    case NODE_STRING_INTERP:
        return check_string_interp_expr(checker, node);

    case NODE_LAMBDA:
        return check_lambda_expr(checker, node);

    case NODE_IS_EXPR:
        return check_is_expr(checker, node);

    default:
        check_error(checker, node->line, node->column, "Unknown expression type %d", node->type);
        return type_error;
    }
}

// Dispatch expression type-checking based on node type, returning the resolved type
Type* check_expression(Checker* checker, Node* node) {
    if (!node)
        return type_error;

    Type* literal_type = get_literal_expr_type(node->type);
    if (literal_type) {
        return literal_type;
    }

    return check_non_literal_expression(checker, node);
}

// =============================================================================
// Struct initializer checking
// =============================================================================

// Type-check a struct initializer: validate field names, types, and completeness
static Type* check_struct_init(Checker* checker, Node* init, Type* struct_type) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT) {
        check_error(checker, init->line, init->column, "Struct initializer requires a struct type");
        return type_error;
    }

    int  field_count = struct_type->as.struc.field_count;
    int* seen        = xcalloc(field_count, sizeof(int));
    int  had_error   = 0;

    for (int i = 0; i < init->as.struct_init.fields.count; i++) {
        Node* field = init->as.struct_init.fields.nodes[i];
        if (!field || field->type != NODE_FIELD_INIT) {
            continue;
        }

        const char* field_name  = field->as.field_init.name;
        int         field_index = -1;

        for (int j = 0; j < field_count; j++) {
            if (strcmp(struct_type->as.struc.field_names[j], field_name) == 0) {
                field_index = j;
                break;
            }
        }

        if (field_index < 0) {
            check_error(checker, field->line, field->column, "Struct '%s' has no field '%s'",
                        struct_type->as.struc.name, field_name);
            had_error = 1;
            continue;
        }

        // Check private field access in struct init
        if (struct_type->as.struc.field_is_private &&
            struct_type->as.struc.field_is_private[field_index]) {
            if (!checker->current_method_receiver ||
                strcmp(struct_type->as.struc.base_name, checker->current_method_receiver) != 0) {
                check_error(checker, field->line, field->column, "Field '%s' is private",
                            field_name);
                had_error = 1;
                continue;
            }
        }

        if (seen[field_index]) {
            check_error(checker, field->line, field->column, "Duplicate initializer for field '%s'",
                        field_name);
            had_error = 1;
            continue;
        }

        seen[field_index] = 1;

        Type* field_type = struct_type->as.struc.field_types[field_index];

        // Set enum_target_hint for generic enum inference (e.g., new Config{ name: Option::None })
        Type* old_hint = checker->enum_target_hint;
        if (field_type->kind == TYPE_ENUM) {
            checker->enum_target_hint = field_type;
        }
        Type* value_type          = check_expression(checker, field->as.field_init.value);
        checker->enum_target_hint = old_hint;

        if (!type_assignable(field_type, value_type)) {
            check_error_type(checker, field->line, field->column, field_name, field_type,
                             value_type);
            had_error = 1;
        }
    }

    for (int i = 0; i < field_count; i++) {
        if (!seen[i]) {
            // Allow Option<T> fields to be omitted (default to None)
            if (type_is_option(struct_type->as.struc.field_types[i])) {
                continue;
            }
            check_error(checker, init->line, init->column, "Missing initializer for field '%s'",
                        struct_type->as.struc.field_names[i]);
            had_error = 1;
        }
    }

    free(seen);
    return had_error ? type_error : struct_type;
}
