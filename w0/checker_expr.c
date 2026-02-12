#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "checker_internal.h"
#include "sem_info.h"

// Forward declaration for check_struct_init (called by check_new_expr)
static Type* check_struct_init(Checker* checker, Node* init, Type* struct_type);

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

// Check comparison operators: == != < > <= >=
static Type* check_comparison_op(Checker* checker, Node* node, Type* left, Type* right) {
    TokenType op = node->as.binary.op;

    // String comparison: only == and != allowed
    if (left->kind == TYPE_STRING && right->kind == TYPE_STRING) {
        if (op == TOK_EQ_EQ || op == TOK_BANG_EQ) {
            node->as.binary.is_string_op = 1;
            return type_bool;
        }
        check_error(checker, node->line, node->column, "Strings only support == and != comparison");
        return type_error;
    }
    if (type_equals(left, right))
        return type_bool;
    // voidptr/struct == null and null == voidptr/struct (only for == and !=)
    if (op == TOK_EQ_EQ || op == TOK_BANG_EQ) {
        if ((left->kind == TYPE_VOIDPTR && right->kind == TYPE_NULL) ||
            (left->kind == TYPE_NULL && right->kind == TYPE_VOIDPTR) ||
            (left->kind == TYPE_STRUCT && right->kind == TYPE_NULL) ||
            (left->kind == TYPE_NULL && right->kind == TYPE_STRUCT)) {
            return type_bool;
        }
    }
    if ((type_is_integer(left) || left->kind == TYPE_F32 || left->kind == TYPE_F64) &&
        (type_is_integer(right) || right->kind == TYPE_F32 || right->kind == TYPE_F64)) {
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

// Type-check a binary expression: dispatch to operator-specific helpers
static Type* check_binary_expr(Checker* checker, Node* node) {
    Type* left  = check_expression(checker, node->as.binary.left);
    Type* right = check_expression(checker, node->as.binary.right);

    if (left->kind == TYPE_ERROR || right->kind == TYPE_ERROR) {
        return type_error;
    }

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

// Type-check a unary expression: negation, logical not, and bitwise complement
static Type* check_unary_expr(Checker* checker, Node* node) {
    Type*     operand = check_expression(checker, node->as.unary.operand);
    TokenType op      = node->as.unary.op;

    if (operand->kind == TYPE_ERROR)
        return type_error;

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
        return object->as.vec.elem;
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
static Type* check_member_module(Checker* checker, Node* node, const char* module_name) {
    // Built-in: std.format(string, ...) -> string
    if (strcmp(module_name, "std") == 0 && strcmp(node->as.member.name, "format") == 0) {
        sem_info_set_member_module_name(checker->sem, node, module_name);
        Type** params = xmalloc(1 * sizeof(Type*));
        params[0]     = type_string;
        return type_func(params, 1, type_string, 1);
    }
    Symbol* sym = checker_lookup_in_module(checker, module_name, node->as.member.name);
    if (!sym) {
        check_error(checker, node->line, node->column, "Module '%s' has no public symbol '%s'",
                    module_name, node->as.member.name);
        return type_error;
    }
    sem_info_set_member_module_name(checker->sem, node, module_name);
    return sym->type;
}

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

// Check Vec member access (count, capacity, data, push, pop, clear)
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
    // Methods: push, pop, clear
    if (strcmp(member_name, "push") == 0 || strcmp(member_name, "pop") == 0 ||
        strcmp(member_name, "clear") == 0) {
        const char* const_name = get_const_binding_name(checker, node->as.member.object);
        if (const_name) {
            check_error(checker, node->line, node->column,
                        "Cannot call mutating method '%s' on const '%s'", member_name, const_name);
            return type_error;
        }
        // Build mangled vec name for method dispatch
        char mangled[256];
        snprintf(mangled, sizeof(mangled), "__Vec_%s", type_mangle_name(elem_type));
        sem_info_set_member_struct_name(checker->sem, node, mangled);

        if (strcmp(member_name, "push") == 0) {
            Type** params = xmalloc(1 * sizeof(Type*));
            params[0]     = elem_type;
            return type_func(params, 1, type_void, 0);
        }
        if (strcmp(member_name, "pop") == 0) {
            return type_func(NULL, 0, elem_type, 0);
        }
        // clear
        return type_func(NULL, 0, type_void, 0);
    }
    check_error(checker, node->line, node->column, "Vec has no member '%s'", member_name);
    return type_error;
}

// Check string member access (length, contains, starts_with, ends_with)
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

// Type-check a member access: dispatch to type-specific helpers
static Type* check_member_expr(Checker* checker, Node* node) {
    // Check for module-qualified access first (e.g., std.print)
    if (node->as.member.object->type == NODE_IDENT) {
        const char* name = node->as.member.object->as.ident.name;
        if (is_imported_module(checker, name))
            return check_member_module(checker, node, name);
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

    // Check if target is assignable (lvalue check)
    Node* t = node->as.assign.target;
    if (!is_lvalue(t)) {
        check_error(checker, node->line, node->column, "Invalid assignment target");
        return type_error;
    }
    if (t->type == NODE_IDENT) {
        Symbol* sym = checker_lookup(checker, t->as.ident.name);
        if (sym && sym->is_const) {
            check_error(checker, node->line, node->column, "Cannot assign to const '%s'",
                        t->as.ident.name);
            return type_error;
        }
    } else if (t->type == NODE_MEMBER) {
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
        if (sem_info_get_member_is_const_access(checker->sem, t, t->as.member.is_const_access)) {
            check_error(checker, node->line, node->column, "Cannot assign to const field '%s'",
                        t->as.member.name);
            return type_error;
        }
    } else if (t->type == NODE_INDEX) {
        const char* const_name = get_const_binding_name(checker, t->as.index.object);
        if (const_name) {
            check_error(checker, node->line, node->column, "Cannot write to index of const '%s'",
                        const_name);
            return type_error;
        }
    }

    // Disallow assignments to data enum tags
    if (t->type == NODE_MEMBER && strcmp(t->as.member.name, "tag") == 0) {
        Type* obj_type = check_expression(checker, t->as.member.object);
        if (obj_type && obj_type->kind == TYPE_ENUM && obj_type->as.enm.has_data) {
            check_error(checker, node->line, node->column,
                        "Enum tag is read-only; cannot assign to '%s.tag'", obj_type->as.enm.name);
            return type_error;
        }
    }

    // For compound assignment, check operation is valid
    TokenType op = node->as.assign.op;
    if (op != TOK_EQ) {
        if ((!type_is_integer(target) && target->kind != TYPE_F32 && target->kind != TYPE_F64) ||
            (!type_is_integer(value) && value->kind != TYPE_F32 && value->kind != TYPE_F64)) {
            check_error(checker, node->line, node->column,
                        "Invalid operands for compound assignment");
            return type_error;
        }
    }

    if (!type_assignable(target, value)) {
        check_error_type(checker, node->line, node->column, "Assignment", target, value);
        return type_error;
    }

    return target;
}

// --- Expression case helpers ---

// Type-check an enum variant constructor, including generic enum type inference
static Type* check_enum_value_expr(Checker* checker, Node* node) {
    const char* enum_name = node->as.enum_value.enum_name;
    Symbol*     sym       = checker_lookup(checker, enum_name);
    Type*       enum_type = NULL;

    if (sym && sym->kind == SYM_TYPE && sym->type->kind == TYPE_ENUM) {
        // Non-generic enum or already-instantiated generic enum
        enum_type = sym->type;
    } else {
        // Try as a generic enum definition
        GenericDef* def = lookup_generic_def(checker, enum_name);
        if (!def || def->decl->type != NODE_ENUM_DECL) {
            check_error(checker, node->line, node->column, "Unknown enum '%s'", enum_name);
            return type_error;
        }

        // Find the variant by name in the template
        Node* template_decl = def->decl;
        int   variant_idx   = -1;
        for (int i = 0; i < template_decl->as.enum_decl.values.count; i++) {
            Node* v = template_decl->as.enum_decl.values.nodes[i];
            if (strcmp(v->as.enum_variant.name, node->as.enum_value.value_name) == 0) {
                variant_idx = i;
                break;
            }
        }
        if (variant_idx < 0) {
            check_error(checker, node->line, node->column, "'%s' is not a value of enum '%s'",
                        node->as.enum_value.value_name, enum_name);
            return type_error;
        }

        Node* variant    = template_decl->as.enum_decl.values.nodes[variant_idx];
        int   type_count = variant->as.enum_variant.types.count;
        int   arg_count  = node->as.enum_value.args.count;

        if (arg_count != type_count) {
            check_error(checker, node->line, node->column,
                        "Enum variant '%s::%s' expects %d argument(s), got %d", enum_name,
                        node->as.enum_value.value_name, type_count, arg_count);
            return type_error;
        }

        // Type-check args and infer type params
        Type** inferred     = xcalloc(def->type_param_count, sizeof(Type*));
        int    all_inferred = 1;

        // First, type-check all args
        Type** arg_types = NULL;
        if (arg_count > 0) {
            arg_types = xmalloc(arg_count * sizeof(Type*));
            for (int i = 0; i < arg_count; i++) {
                arg_types[i] = check_expression(checker, node->as.enum_value.args.nodes[i]);
            }
        }

        // Infer type params from args
        for (int i = 0; i < arg_count; i++) {
            Node* vtype_node = variant->as.enum_variant.types.nodes[i];
            if (vtype_node->type == NODE_IDENT) {
                // Check if this ident is a type param
                for (int p = 0; p < def->type_param_count; p++) {
                    if (strcmp(vtype_node->as.ident.name, def->type_params[p]) == 0) {
                        if (!inferred[p]) {
                            inferred[p] = arg_types[i];
                        }
                        break;
                    }
                }
            }
        }

        // Check if all type params were inferred
        for (int p = 0; p < def->type_param_count; p++) {
            if (!inferred[p]) {
                all_inferred = 0;
                break;
            }
        }

        // If not all inferred, try the target hint
        if (!all_inferred && checker->enum_target_hint &&
            checker->enum_target_hint->kind == TYPE_ENUM) {
            // The target hint is an already-instantiated generic enum
            // Find the instance to get the type args
            for (int gi = 0; gi < checker->generics.instance_count; gi++) {
                GenericInstance* inst = &checker->generics.instances[gi];
                if (inst->type == checker->enum_target_hint &&
                    strcmp(inst->base_name, enum_name) == 0) {
                    // Use instance type args for any missing inferred params
                    for (int p = 0; p < def->type_param_count && p < inst->type_arg_count; p++) {
                        if (!inferred[p]) {
                            inferred[p] = inst->type_args[p];
                        }
                    }
                    break;
                }
            }
            // Recheck
            all_inferred = 1;
            for (int p = 0; p < def->type_param_count; p++) {
                if (!inferred[p]) {
                    all_inferred = 0;
                    break;
                }
            }
        }

        if (!all_inferred) {
            check_error(checker, node->line, node->column,
                        "Cannot infer type parameters for generic enum '%s'; add explicit type "
                        "annotation",
                        enum_name);
            free(inferred);
            free(arg_types);
            return type_error;
        }

        // Generate mangled name and instantiate
        char* mangled = type_mangle_generic(enum_name, inferred, def->type_param_count);

        GenericInstance* existing = lookup_generic_instance(checker, mangled);
        if (existing) {
            enum_type = existing->type;
            free(mangled);
        } else {
            // Make a copy of inferred for instantiate (it takes ownership)
            Type** args_copy = xmalloc(def->type_param_count * sizeof(Type*));
            for (int p = 0; p < def->type_param_count; p++) {
                args_copy[p] = inferred[p];
            }
            enum_type =
                instantiate_generic_enum(checker, def, mangled, args_copy, def->type_param_count);
        }

        free(inferred);
        free(arg_types);
    }

    sem_info_set_enum_value_resolved_name(checker->sem, node, enum_type->as.enm.name);

    // Check that the value exists in the (possibly instantiated) enum
    int variant_idx = -1;
    for (int i = 0; i < enum_type->as.enm.value_count; i++) {
        if (strcmp(enum_type->as.enm.value_names[i], node->as.enum_value.value_name) == 0) {
            variant_idx = i;
            break;
        }
    }
    if (variant_idx < 0) {
        check_error(checker, node->line, node->column, "'%s' is not a value of enum '%s'",
                    node->as.enum_value.value_name, enum_type->as.enm.name);
        return type_error;
    }

    // Set is_data_enum flag for codegen
    sem_info_set_enum_value_is_data_enum(checker->sem, node, enum_type->as.enm.has_data);

    // Validate constructor args for data enums
    int expected_args = enum_type->as.enm.variant_type_counts[variant_idx];
    int actual_args   = node->as.enum_value.args.count;

    if (actual_args != expected_args) {
        check_error(checker, node->line, node->column,
                    "Enum variant '%s::%s' expects %d argument(s), got %d", enum_type->as.enm.name,
                    node->as.enum_value.value_name, expected_args, actual_args);
        return type_error;
    }

    // Type-check each constructor arg
    for (int i = 0; i < actual_args; i++) {
        Type* arg_type = check_expression(checker, node->as.enum_value.args.nodes[i]);
        Type* expected = enum_type->as.enm.variant_types[variant_idx][i];
        if (arg_type->kind != TYPE_ERROR && !type_assignable(expected, arg_type)) {
            check_error(checker, node->as.enum_value.args.nodes[i]->line,
                        node->as.enum_value.args.nodes[i]->column,
                        "Enum variant '%s::%s' argument %d: expected '%s', got '%s'",
                        enum_type->as.enm.name, node->as.enum_value.value_name, i + 1,
                        type_name(expected), type_name(arg_type));
        }
    }

    return enum_type;
}

// Type-check a function call: validate callee, argument count, and argument types
static Type* check_call_expr(Checker* checker, Node* node) {
    Type* func_type = check_expression(checker, node->as.call.func);

    if (func_type->kind == TYPE_ERROR)
        return type_error;

    if (func_type->kind != TYPE_FUNC) {
        check_error_cannot(checker, node->line, node->column, "call", func_type);
        return type_error;
    }

    // Check argument count
    if (func_type->as.func.is_varargs) {
        // Varargs: require at least param_count arguments
        if (node->as.call.args.count < func_type->as.func.param_count) {
            check_error(checker, node->line, node->column, "Expected at least %d arguments, got %d",
                        func_type->as.func.param_count, node->as.call.args.count);
            return type_error;
        }
    } else {
        if (node->as.call.args.count != func_type->as.func.param_count) {
            check_error(checker, node->line, node->column, "Expected %d arguments, got %d",
                        func_type->as.func.param_count, node->as.call.args.count);
            return type_error;
        }
    }

    // Check argument types (only for named params)
    for (int i = 0; i < func_type->as.func.param_count; i++) {
        Type* arg_type   = check_expression(checker, node->as.call.args.nodes[i]);
        Type* param_type = func_type->as.func.param_types[i];

        if (!type_assignable(param_type, arg_type)) {
            char ctx[32];
            snprintf(ctx, sizeof(ctx), "Argument %d", i + 1);
            check_error_type(checker, node->as.call.args.nodes[i]->line,
                             node->as.call.args.nodes[i]->column, ctx, param_type, arg_type);
        }
    }

    // Type-check extra variadic arguments (but don't check against param types)
    for (int i = func_type->as.func.param_count; i < node->as.call.args.count; i++) {
        check_expression(checker, node->as.call.args.nodes[i]);
    }

    return func_type->as.func.return_type;
}

// Type-check a `new` expression: resolve type, validate struct init or Vec elements
static Type* check_new_expr(Checker* checker, Node* node) {
    Type* resolved = resolve_type(checker, node->as.new_expr.type_node);
    if (resolved == type_error)
        return type_error;
    if (resolved->kind == TYPE_VEC) {
        // new Vec<T>{} or new Vec<T>{1, 2, 3}
        Type* elem_type = resolved->as.vec.elem;
        Node* init      = node->as.new_expr.init;
        // Check each element expression against the element type
        for (int i = 0; i < init->as.struct_init.fields.count; i++) {
            Node* field = init->as.struct_init.fields.nodes[i];
            if (field->type != NODE_FIELD_INIT)
                continue;
            Type* val_type = check_expression(checker, field->as.field_init.value);
            if (val_type->kind != TYPE_ERROR && !type_assignable(elem_type, val_type)) {
                check_error_type(checker, field->line, field->column, "Vec element", elem_type,
                                 val_type);
            }
        }
        node->as.new_expr.resolved_type = resolved;
        return resolved;
    }
    if (resolved->kind != TYPE_STRUCT) {
        check_error(checker, node->line, node->column, "'new' requires a struct type, got '%s'",
                    type_name(resolved));
        return type_error;
    }
    Type* init_type = check_struct_init(checker, node->as.new_expr.init, resolved);
    if (init_type == type_error)
        return type_error;
    node->as.new_expr.resolved_type = resolved;
    return resolved;
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
    return type_tuple(elems, count);
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
    return type_string;
}

// =============================================================================
// Try expression checking (? operator)
// =============================================================================

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

    // Detect Option pattern: has Some(T) + None variants
    // Detect Result pattern: has Ok(T) + Err(E) variants
    int ok_idx = -1, err_idx = -1, some_idx = -1, none_idx = -1;
    for (int i = 0; i < expr_type->as.enm.value_count; i++) {
        const char* vname = expr_type->as.enm.value_names[i];
        if (strcmp(vname, "Ok") == 0)
            ok_idx = i;
        else if (strcmp(vname, "Err") == 0)
            err_idx = i;
        else if (strcmp(vname, "Some") == 0)
            some_idx = i;
        else if (strcmp(vname, "None") == 0)
            none_idx = i;
    }

    int is_result = (ok_idx >= 0 && err_idx >= 0);
    int is_option = (some_idx >= 0 && none_idx >= 0);

    // Result takes priority if enum has both Ok/Err and Some/None
    if (is_result)
        is_option = 0;

    if (!is_result && !is_option) {
        check_error(checker, node->line, node->column,
                    "'?' operator requires a Result (Ok/Err) or Option (Some/None) enum, got '%s'",
                    type_name(expr_type));
        return type_error;
    }

    // Extract unwrapped type T from Ok(T) or Some(T)
    int success_idx = is_result ? ok_idx : some_idx;
    if (expr_type->as.enm.variant_type_counts[success_idx] != 1) {
        check_error(checker, node->line, node->column,
                    "'?' requires '%s' variant to have exactly one payload field",
                    is_result ? "Ok" : "Some");
        return type_error;
    }
    Type* unwrapped_type = expr_type->as.enm.variant_types[success_idx][0];

    // Validate function return type compatibility
    Type* ret_type = checker->current_func_return;
    if (ret_type->kind != TYPE_ENUM || !ret_type->as.enm.has_data) {
        check_error(checker, node->line, node->column,
                    "'?' used in function returning '%s', but must return a Result or Option type",
                    type_name(ret_type));
        return type_error;
    }

    if (is_option) {
        // Option: return type must have a None variant
        int ret_has_none = 0;
        for (int i = 0; i < ret_type->as.enm.value_count; i++) {
            if (strcmp(ret_type->as.enm.value_names[i], "None") == 0) {
                ret_has_none = 1;
                break;
            }
        }
        if (!ret_has_none) {
            check_error(checker, node->line, node->column,
                        "'?' on Option requires function return type to have 'None' variant, "
                        "got '%s'",
                        type_name(ret_type));
            return type_error;
        }
    } else {
        // Result: return type must have an Err variant with matching error type
        int ret_err_idx = -1;
        for (int i = 0; i < ret_type->as.enm.value_count; i++) {
            if (strcmp(ret_type->as.enm.value_names[i], "Err") == 0) {
                ret_err_idx = i;
                break;
            }
        }
        if (ret_err_idx < 0) {
            check_error(checker, node->line, node->column,
                        "'?' on Result requires function return type to have 'Err' variant, "
                        "got '%s'",
                        type_name(ret_type));
            return type_error;
        }
        // Check error type compatibility
        if (expr_type->as.enm.variant_type_counts[err_idx] != 1 ||
            ret_type->as.enm.variant_type_counts[ret_err_idx] != 1) {
            check_error(checker, node->line, node->column,
                        "'?' requires 'Err' variant to have exactly one payload field");
            return type_error;
        }
        Type* expr_err_type = expr_type->as.enm.variant_types[err_idx][0];
        Type* ret_err_type  = ret_type->as.enm.variant_types[ret_err_idx][0];
        if (!type_assignable(ret_err_type, expr_err_type)) {
            check_error(checker, node->line, node->column,
                        "'?' error type mismatch: expression has Err(%s) but function returns "
                        "Err(%s)",
                        type_name(expr_err_type), type_name(ret_err_type));
            return type_error;
        }
    }

    // Set checker fields on the node
    node->as.try_expr.resolved_type  = expr_type;
    node->as.try_expr.unwrapped_type = unwrapped_type;
    node->as.try_expr.is_option      = is_option ? 1 : 0;
    node->as.try_expr.enum_name      = xstrdup(expr_type->as.enm.name);
    node->as.try_expr.ret_enum_name  = xstrdup(ret_type->as.enm.name);

    return unwrapped_type;
}

// =============================================================================
// Expression checking
// =============================================================================

// Dispatch expression type-checking based on node type, returning the resolved type
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
        check_error(checker, node->line, node->column,
                    "Struct initializer requires a contextual struct type");
        return type_error;

    case NODE_TUPLE_LIT:
        return check_tuple_lit_expr(checker, node);

    case NODE_ARRAY_LIT:
        return check_array_lit_expr(checker, node);

    case NODE_STRING_INTERP:
        return check_string_interp_expr(checker, node);

    default:
        check_error(checker, node->line, node->column, "Unknown expression type %d", node->type);
        return type_error;
    }
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

        if (seen[field_index]) {
            check_error(checker, field->line, field->column, "Duplicate initializer for field '%s'",
                        field_name);
            had_error = 1;
            continue;
        }

        seen[field_index] = 1;

        Type* value_type = check_expression(checker, field->as.field_init.value);
        Type* field_type = struct_type->as.struc.field_types[field_index];

        if (!type_assignable(field_type, value_type)) {
            check_error_type(checker, field->line, field->column, field_name, field_type,
                             value_type);
            had_error = 1;
        }
    }

    for (int i = 0; i < field_count; i++) {
        if (!seen[i]) {
            check_error(checker, init->line, init->column, "Missing initializer for field '%s'",
                        struct_type->as.struc.field_names[i]);
            had_error = 1;
        }
    }

    free(seen);
    return had_error ? type_error : struct_type;
}
