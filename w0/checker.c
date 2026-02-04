#include "checker.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"

#define SCOPE_SIZE 64

// Forward declarations for mutually recursive functions
static void  check_decl(Checker* checker, Node* node);
static void  check_statement(Checker* checker, Node* node);
static Type* check_expression(Checker* checker, Node* node);
static Type* check_struct_init(Checker* checker, Node* init, Type* struct_type);
static Type* resolve_type(Checker* checker, Node* type_node);

// Forward declarations for destructuring pattern checking
static int  check_destruct_pattern_redefinitions(Checker* checker, DestructPattern* pattern,
                                                 int line, int col);
static int  check_destruct_pattern_against_type(Checker* checker, DestructPattern* pattern,
                                                Type* type, int line, int col);
static void define_destruct_pattern_vars(Checker* checker, DestructPattern* pattern, Type* type,
                                         int is_const, int is_public);

// =============================================================================
// Utility functions
// =============================================================================

static void check_error(Checker* checker, int line, int col, const char* fmt, ...) {
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

// =============================================================================
// Checker initialization and cleanup
// =============================================================================

void checker_init(Checker* checker) {
    checker->scope                            = NULL;
    checker->current_func_return              = NULL;
    checker->in_loop                          = 0;
    checker->error_count                      = 0;
    checker->error_msg[0]                     = '\0';
    checker->direct_imports                   = NULL;
    checker->direct_imports_count             = 0;
    checker->current_accessible_modules       = NULL;
    checker->current_accessible_modules_count = 0;
    checker->current_module                   = NULL;
    types_init();
}

void checker_set_direct_imports(Checker* checker, char** direct_imports, int count) {
    checker->direct_imports       = direct_imports;
    checker->direct_imports_count = count;
}

void checker_push_scope(Checker* checker) {
    Scope* scope   = xcalloc(1, sizeof(Scope));
    scope->symbols = xcalloc(SCOPE_SIZE, sizeof(Symbol*));
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
            free(sym->source_module);
            free(sym);
            sym = next;
        }
    }
    free(scope->symbols);
    free(scope);
}

void checker_free(Checker* checker) {
    while (checker->scope) {
        checker_pop_scope(checker);
    }
    types_cleanup();
}

// =============================================================================
// Symbol table operations
// =============================================================================

Symbol* checker_define(Checker* checker, const char* name, SymbolKind kind, Type* type,
                       int is_const, int is_public, const char* source_module) {
    Scope*       scope = checker->scope;
    unsigned int index = hash_string(name) % scope->size;

    // Check for redefinition in current scope
    for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
        if (strcmp(sym->name, name) == 0) {
            return NULL; // Already defined
        }
    }

    Symbol* sym           = xcalloc(1, sizeof(Symbol));
    sym->kind             = kind;
    sym->name             = xstrdup(name);
    sym->type             = type;
    sym->is_const         = is_const;
    sym->is_public        = is_public;
    sym->source_module    = source_module ? xstrdup(source_module) : NULL;
    sym->next             = scope->symbols[index];
    scope->symbols[index] = sym;
    return sym;
}

// Check if a module is accessible from the current context
static int is_module_accessible(Checker* checker, const char* source_module) {
    // NULL source_module means same module - always accessible
    if (!source_module) {
        return 1;
    }

    // If we're currently in the same module, it's accessible
    if (checker->current_module && strcmp(checker->current_module, source_module) == 0) {
        return 1;
    }

    // Use current function's accessible modules if set, otherwise use global direct_imports
    char** modules = checker->current_accessible_modules;
    int    count   = checker->current_accessible_modules_count;

    if (!modules) {
        modules = checker->direct_imports;
        count   = checker->direct_imports_count;
    }

    // Check if module is in the accessible list
    for (int i = 0; i < count; i++) {
        if (strcmp(modules[i], source_module) == 0) {
            return 1;
        }
    }

    return 0;
}

// Check if a name is a directly imported library module
int is_imported_module(Checker* checker, const char* name) {
    // Use current function's accessible modules if set, otherwise use global direct_imports
    char** modules = checker->current_accessible_modules;
    int    count   = checker->current_accessible_modules_count;

    if (!modules) {
        modules = checker->direct_imports;
        count   = checker->direct_imports_count;
    }

    for (int i = 0; i < count; i++) {
        if (strcmp(modules[i], name) == 0) {
            return 1;
        }
    }
    return 0;
}

// Look up a symbol from a specific module (for module-qualified access)
Symbol* checker_lookup_in_module(Checker* checker, const char* module_name,
                                 const char* symbol_name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(symbol_name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, symbol_name) == 0 && sym->source_module &&
                strcmp(sym->source_module, module_name) == 0) {
                // Must be public for module-qualified access
                if (!sym->is_public) {
                    return NULL;
                }
                return sym;
            }
        }
    }
    return NULL;
}

Symbol* checker_lookup(Checker* checker, const char* name) {
    for (Scope* scope = checker->scope; scope; scope = scope->parent) {
        unsigned int index = hash_string(name) % scope->size;
        for (Symbol* sym = scope->symbols[index]; sym; sym = sym->next) {
            if (strcmp(sym->name, name) == 0) {
                // Check module visibility
                if (!is_module_accessible(checker, sym->source_module)) {
                    continue; // Symbol exists but not accessible from this module
                }
                // Symbols from library imports require module qualification
                // Skip them here - they must be accessed via module.symbol syntax
                int same_module = (sym->source_module == NULL && checker->current_module == NULL) ||
                                  (sym->source_module && checker->current_module &&
                                   strcmp(sym->source_module, checker->current_module) == 0);
                if (sym->source_module && !same_module) {
                    continue; // Library symbol - require module qualification
                }
                return sym;
            }
        }
    }
    return NULL;
}

// Lookup without visibility checking (for error messages)
Symbol* checker_lookup_any(Checker* checker, const char* name) {
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

// =============================================================================
// Type resolution
// =============================================================================

static Type* resolve_type(Checker* checker, Node* type_node) {
    if (!type_node)
        return type_void;

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;

        // Check for builtin type
        Type* builtin = type_builtin_from_name(name);
        if (builtin)
            return builtin;

        // Look up user-defined type
        Symbol* sym = checker_lookup(checker, name);
        if (sym && sym->kind == SYM_TYPE) {
            return sym->type;
        }
        check_error(checker, type_node->line, type_node->column, "Unknown type '%s'", name);
        return type_error;
    }
    case NODE_UNARY:
        // Pointer types no longer supported
        check_error(checker, type_node->line, type_node->column,
                    "Pointer types are no longer supported");
        return type_error;
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
    case NODE_TUPLE_TYPE: {
        int    count = type_node->as.tuple_type.elem_types.count;
        Type** elems = xmalloc(count * sizeof(Type*));
        for (int i = 0; i < count; i++) {
            elems[i] = resolve_type(checker, type_node->as.tuple_type.elem_types.nodes[i]);
        }
        return type_tuple(elems, count);
    }
    default:
        break;
    }

    check_error(checker, type_node->line, type_node->column, "Invalid type");
    return type_error;
}

// =============================================================================
// Expression checking
// =============================================================================

static Type* check_expression(Checker* checker, Node* node) {
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
                node->as.member.module_name = xstrdup(name);
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
                node->as.member.struct_name = xstrdup(object->as.struc.name);
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
        Type** elems = xmalloc(count * sizeof(Type*));
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

// =============================================================================
// Struct initializer checking
// =============================================================================

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
            check_error(checker, field->line, field->column,
                        "Cannot initialize field '%s' of type '%s' with '%s'", field_name,
                        type_name(field_type), type_name(value_type));
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

// =============================================================================
// Destructuring pattern checking
// =============================================================================

// Check for redefinitions in a destructuring pattern (recursive)
// Returns 1 if error found, 0 otherwise
static int check_destruct_pattern_redefinitions(Checker* checker, DestructPattern* pattern,
                                                int line, int col) {
    if (!pattern)
        return 0;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        if (checker_lookup_local(checker, pattern->as.ident.name)) {
            check_error(checker, line, col, "Redefinition of '%s'", pattern->as.ident.name);
            return 1;
        }
        return 0;

    case PATTERN_TUPLE:
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            if (check_destruct_pattern_redefinitions(checker, pattern->as.tuple.elements[i], line,
                                                     col)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// Check that a destructuring pattern matches a type (recursive)
// Also sets resolved_type on each pattern node
// Returns 1 if error found, 0 otherwise
static int check_destruct_pattern_against_type(Checker* checker, DestructPattern* pattern,
                                               Type* type, int line, int col) {
    if (!pattern || !type)
        return 1;

    pattern->resolved_type = type;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        // Any type is valid for an identifier pattern
        return 0;

    case PATTERN_TUPLE:
        // Pattern must match a tuple type
        if (type->kind != TYPE_TUPLE && type->kind != TYPE_ERROR) {
            check_error(checker, line, col, "Nested pattern requires a tuple, got '%s'",
                        type_name(type));
            return 1;
        }

        if (type->kind == TYPE_ERROR) {
            // Propagate error type to all children
            for (int i = 0; i < pattern->as.tuple.count; i++) {
                check_destruct_pattern_against_type(checker, pattern->as.tuple.elements[i],
                                                    type_error, line, col);
            }
            return 0;
        }

        // Check arity
        if (type->as.tuple.elem_count != pattern->as.tuple.count) {
            check_error(checker, line, col,
                        "Nested pattern has %d elements, but tuple has %d elements",
                        pattern->as.tuple.count, type->as.tuple.elem_count);
            return 1;
        }

        // Recursively check each element
        for (int i = 0; i < pattern->as.tuple.count; i++) {
            if (check_destruct_pattern_against_type(checker, pattern->as.tuple.elements[i],
                                                    type->as.tuple.elem_types[i], line, col)) {
                return 1;
            }
        }
        return 0;
    }
    return 0;
}

// Define variables for all identifiers in a destructuring pattern (recursive)
static void define_destruct_pattern_vars(Checker* checker, DestructPattern* pattern, Type* type,
                                         int is_const, int is_public) {
    if (!pattern)
        return;

    switch (pattern->kind) {
    case PATTERN_IDENT:
        checker_define(checker, pattern->as.ident.name, SYM_VAR, type, is_const, is_public,
                       checker->current_module);
        break;

    case PATTERN_TUPLE:
        if (type->kind == TYPE_TUPLE) {
            for (int i = 0; i < pattern->as.tuple.count; i++) {
                define_destruct_pattern_vars(checker, pattern->as.tuple.elements[i],
                                             type->as.tuple.elem_types[i], is_const, is_public);
            }
        } else {
            // Error type - propagate to all children
            for (int i = 0; i < pattern->as.tuple.count; i++) {
                define_destruct_pattern_vars(checker, pattern->as.tuple.elements[i], type_error,
                                             is_const, is_public);
            }
        }
        break;
    }
}

// =============================================================================
// Statement checking
// =============================================================================

static void check_statement(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXPR_STMT:
        check_expression(checker, node->as.expr_stmt.expr);
        break;

    case NODE_VAR_DECL: {
        // Check if this is a destructuring declaration
        DestructPattern* pattern = node->as.var_decl.destruct_pattern;
        if (pattern) {
            // Destructuring: var (a, b) = tuple; or var (a, (b, c)) = nested;

            // Check for redefinitions
            if (check_destruct_pattern_redefinitions(checker, pattern, node->line, node->column)) {
                return;
            }

            // Initializer is required (parser enforces this)
            Type* init_type = check_expression(checker, node->as.var_decl.init);

            // Check pattern against initializer type
            if (check_destruct_pattern_against_type(checker, pattern, init_type, node->line,
                                                    node->column)) {
                return;
            }

            // Define all variables in the pattern
            define_destruct_pattern_vars(checker, pattern, init_type, node->as.var_decl.is_const,
                                         node->as.var_decl.is_public);
            break;
        }

        // Normal variable declaration
        const char* name = node->as.var_decl.name;

        // Check for redefinition
        if (checker_lookup_local(checker, name)) {
            check_error(checker, node->line, node->column, "Redefinition of '%s'", name);
            return;
        }

        Type* decl_type = NULL;
        Type* init_type = NULL;

        if (node->as.var_decl.type) {
            decl_type = resolve_type(checker, node->as.var_decl.type);
        }

        if (node->as.var_decl.init) {
            if (node->as.var_decl.init->type == NODE_STRUCT_INIT) {
                if (!decl_type) {
                    check_error(checker, node->line, node->column,
                                "Struct initializer requires an explicit type");
                    init_type = type_error;
                } else {
                    init_type = check_struct_init(checker, node->as.var_decl.init, decl_type);
                }
            } else {
                init_type = check_expression(checker, node->as.var_decl.init);
            }
        }

        Type* var_type;
        if (decl_type && init_type) {
            // Both specified - check compatibility
            if (!type_assignable(decl_type, init_type)) {
                check_error(checker, node->line, node->column,
                            "Cannot initialize '%s' of type '%s' with '%s'", name,
                            type_name(decl_type), type_name(init_type));
            }
            var_type = decl_type;
        } else if (decl_type) {
            var_type = decl_type;
        } else if (init_type) {
            var_type = init_type;
        } else {
            check_error(checker, node->line, node->column,
                        "Variable '%s' needs type annotation or initializer", name);
            var_type = type_error;
        }

        checker_define(checker, name, SYM_VAR, var_type, node->as.var_decl.is_const,
                       node->as.var_decl.is_public, checker->current_module);
        break;
    }

    case NODE_BLOCK:
        checker_push_scope(checker);
        for (int i = 0; i < node->as.block.stmts.count; i++) {
            check_statement(checker, node->as.block.stmts.nodes[i]);
        }
        checker_pop_scope(checker);
        break;

    case NODE_IF: {
        Type* cond = check_expression(checker, node->as.if_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.if_stmt.cond->line, node->as.if_stmt.cond->column,
                        "If condition must be bool, got '%s'", type_name(cond));
        }
        check_statement(checker, node->as.if_stmt.then_block);
        if (node->as.if_stmt.else_block) {
            check_statement(checker, node->as.if_stmt.else_block);
        }
        break;
    }

    case NODE_WHILE: {
        Type* cond = check_expression(checker, node->as.while_stmt.cond);
        if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
            check_error(checker, node->as.while_stmt.cond->line, node->as.while_stmt.cond->column,
                        "While condition must be bool, got '%s'", type_name(cond));
        }
        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.while_stmt.body);
        checker->in_loop = was_in_loop;
        break;
    }

    case NODE_FOR: {
        checker_push_scope(checker); // New scope for init var

        if (node->as.for_stmt.init) {
            check_statement(checker, node->as.for_stmt.init);
        }

        if (node->as.for_stmt.cond) {
            Type* cond = check_expression(checker, node->as.for_stmt.cond);
            if (cond->kind != TYPE_BOOL && cond->kind != TYPE_ERROR) {
                check_error(checker, node->as.for_stmt.cond->line, node->as.for_stmt.cond->column,
                            "For condition must be bool, got '%s'", type_name(cond));
            }
        }

        if (node->as.for_stmt.post) {
            check_expression(checker, node->as.for_stmt.post);
        }

        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.for_stmt.body);
        checker->in_loop = was_in_loop;

        checker_pop_scope(checker);
        break;
    }

    case NODE_FOREACH: {
        checker_push_scope(checker); // New scope for loop variable

        // Check that start and end are integers
        Type* start_type = check_expression(checker, node->as.foreach_stmt.start);
        Type* end_type   = check_expression(checker, node->as.foreach_stmt.end);

        if (start_type->kind != TYPE_INT64 && start_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.start->line,
                        node->as.foreach_stmt.start->column,
                        "Foreach range start must be int, got '%s'", type_name(start_type));
        }

        if (end_type->kind != TYPE_INT64 && end_type->kind != TYPE_ERROR) {
            check_error(checker, node->as.foreach_stmt.end->line, node->as.foreach_stmt.end->column,
                        "Foreach range end must be int, got '%s'", type_name(end_type));
        }

        // Add the loop variable as a const int64 (immutable)
        Symbol* sym = checker_define(checker, node->as.foreach_stmt.var_name, SYM_VAR, type_int64,
                                     1, 0, NULL);
        if (!sym) {
            check_error(checker, node->line, node->column,
                        "Variable '%s' already declared in this scope",
                        node->as.foreach_stmt.var_name);
        }

        int was_in_loop  = checker->in_loop;
        checker->in_loop = 1;
        check_statement(checker, node->as.foreach_stmt.body);
        checker->in_loop = was_in_loop;

        checker_pop_scope(checker);
        break;
    }

    case NODE_RETURN: {
        Type* expected = checker->current_func_return;
        if (!expected) {
            check_error(checker, node->line, node->column, "Return outside of function");
            return;
        }

        if (node->as.return_stmt.value) {
            Type* actual = check_expression(checker, node->as.return_stmt.value);
            if (!type_assignable(expected, actual)) {
                check_error(checker, node->line, node->column,
                            "Return type mismatch: expected '%s', got '%s'", type_name(expected),
                            type_name(actual));
            }
        } else if (expected->kind != TYPE_VOID) {
            check_error(checker, node->line, node->column,
                        "Return without value in non-void function");
        }
        break;
    }

    case NODE_BREAK:
        if (!checker->in_loop) {
            check_error(checker, node->line, node->column, "Break outside of loop");
        }
        break;

    case NODE_CONTINUE:
        if (!checker->in_loop) {
            check_error(checker, node->line, node->column, "Continue outside of loop");
        }
        break;

    case NODE_DEFER:
        if (!checker->current_func_return) {
            check_error(checker, node->line, node->column, "Defer outside of function");
            return;
        }
        check_statement(checker, node->as.defer_stmt.stmt);
        break;

    default:
        check_error(checker, node->line, node->column, "Unknown statement type %d", node->type);
        break;
    }
}

// =============================================================================
// Declaration checking
// =============================================================================

static Type* get_function_type(Checker* checker, Node* node) {
    func_decl_node* fdn = &(node->as.func_decl);

    int    param_count = fdn->params.count;
    Type** param_types = NULL;
    if (param_count > 0) {
        param_types = xmalloc(param_count * sizeof(Type*));
    }

    Type* return_type = type_void;
    if (fdn->return_type) {
        return_type = resolve_type(checker, fdn->return_type);
    }

    // Build function type
    for (int i = 0; i < param_count; i++) {
        Node* param = fdn->params.nodes[i];
        Type* ptype = type_void;
        if (param->as.param.type) {
            ptype = resolve_type(checker, param->as.param.type);
        }
        param_types[i] = ptype;
    }

    Type* func_type = type_func(param_types, param_count, return_type);

    return func_type;
}

static void check_decl(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_EXTERN_MODULE: {
        for (int i = 0; i < node->as.extern_module.decls.count; i++) {
            Node* decl = node->as.extern_module.decls.nodes[i];
            if (decl->type == NODE_FUNC_DECL) {
                func_decl_node* fdn       = &decl->as.func_decl;
                Type*           func_type = get_function_type(checker, decl);

                checker_define(checker, fdn->name, SYM_FUNC, func_type, 0, fdn->is_public,
                               checker->current_module);
            }
        }

        break;
    }

    case NODE_FUNC_DECL: {
        func_decl_node* fdn = &node->as.func_decl;

        const char* name          = fdn->name;
        const char* receiver_type = fdn->receiver_type;
        int         is_method     = (receiver_type != NULL);

        // For methods, use mangled name: StructName_methodName
        char* mangled_name = NULL;
        if (is_method) {
            size_t len   = strlen(receiver_type) + 1 + strlen(name) + 1;
            mangled_name = xmalloc(len);
            snprintf(mangled_name, len, "%s_%s", receiver_type, name);
        } else {
            mangled_name = xstrdup(name);
        }

        // if main function, ensure correct signature
        if (!is_method && strcmp(name, "main") == 0) {
            if (fdn->params.count != 0) {
                check_error(checker, node->line, node->column,
                            "main function must not have parameters");
            }
            if (fdn->return_type) {
                Type* ret_type = resolve_type(checker, fdn->return_type);
                if (ret_type->kind != TYPE_INT32) {
                    check_error(checker, node->line, node->column,
                                "main function must have return type i32");
                }
            } else {
                check_error(checker, node->line, node->column,
                            "main function must have return type i32");
            }
        }

        // Check for redefinition
        if (checker_lookup(checker, mangled_name)) {
            check_error(checker, node->line, node->column, "Redefinition of '%s'", mangled_name);
            free(mangled_name);
            return;
        }

        Type* func_type = get_function_type(checker, node);

        // Pre-declare function for recursion
        checker_define(checker, mangled_name, SYM_FUNC, func_type, 1, fdn->is_public,
                       checker->current_module);

        // For methods, also register the method on the struct type
        if (is_method) {
            Symbol* struct_sym = checker_lookup(checker, receiver_type);
            if (!struct_sym || struct_sym->kind != SYM_TYPE ||
                struct_sym->type->kind != TYPE_STRUCT) {
                check_error(checker, node->line, node->column, "Unknown receiver type '%s'",
                            receiver_type);
            } else {
                Type* st = struct_sym->type;
                int   n  = st->as.struc.method_count;

                st->as.struc.method_names =
                    xrealloc(st->as.struc.method_names, (n + 1) * sizeof(char*));
                st->as.struc.method_types =
                    xrealloc(st->as.struc.method_types, (n + 1) * sizeof(Type*));
                st->as.struc.method_is_const =
                    xrealloc(st->as.struc.method_is_const, (n + 1) * sizeof(int));

                st->as.struc.method_names[n]    = xstrdup(name);
                st->as.struc.method_types[n]    = func_type;
                st->as.struc.method_is_const[n] = fdn->receiver_is_const;
                st->as.struc.method_count       = n + 1;
            }
        }

        // Enter function scope
        checker_push_scope(checker);
        Type* old_return             = checker->current_func_return;
        checker->current_func_return = func_type->as.func.return_type;

        // Set this function's accessible modules for visibility checking
        char** old_accessible_modules             = checker->current_accessible_modules;
        int    old_accessible_modules_count       = checker->current_accessible_modules_count;
        checker->current_accessible_modules       = fdn->accessible_modules;
        checker->current_accessible_modules_count = fdn->accessible_modules_count;

        // For methods, inject 'self' into scope
        // self is a struct reference (the struct type itself, with reference semantics)
        if (is_method) {
            Symbol* struct_sym = checker_lookup(checker, receiver_type);
            if (struct_sym && struct_sym->kind == SYM_TYPE) {
                Type* self_type = struct_sym->type;
                checker_define(checker, "self", SYM_VAR, self_type, fdn->receiver_is_const, 0,
                               NULL);
            }
        }

        // Define parameters
        for (int i = 0; i < func_type->as.func.param_count; i++) {
            Node* param = fdn->params.nodes[i];
            Type* ptype = func_type->as.func.param_types[i];

            if (!checker_define(checker, param->as.param.name, SYM_VAR, ptype,
                                param->as.param.is_const, 0, NULL)) {
                check_error(checker, param->line, param->column, "Duplicate parameter name '%s'",
                            param->as.param.name);
            }
        }

        // Check body
        if (fdn->body) {
            // Body is a block, but we already pushed scope for params
            // So just check the statements directly
            Node* body = fdn->body;
            for (int i = 0; i < body->as.block.stmts.count; i++) {
                check_statement(checker, body->as.block.stmts.nodes[i]);
            }
        }

        // Restore previous accessible modules context
        checker->current_accessible_modules       = old_accessible_modules;
        checker->current_accessible_modules_count = old_accessible_modules_count;

        checker->current_func_return = old_return;
        checker_pop_scope(checker);
        free(mangled_name);
        break;
    }

    case NODE_STRUCT_DECL: {
        const char* name = node->as.struct_decl.name;

        if (checker_lookup(checker, name)) {
            check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
            return;
        }

        Type* struct_type = type_struct(name);
        int   field_count = node->as.struct_decl.fields.count;

        struct_type->as.struc.field_count = field_count;
        struct_type->as.struc.field_names = xmalloc(field_count * sizeof(char*));
        struct_type->as.struc.field_types = xmalloc(field_count * sizeof(Type*));

        for (int i = 0; i < field_count; i++) {
            Node* field                          = node->as.struct_decl.fields.nodes[i];
            struct_type->as.struc.field_names[i] = xstrdup(field->as.field.name);
            struct_type->as.struc.field_types[i] = resolve_type(checker, field->as.field.type);
        }

        checker_define(checker, name, SYM_TYPE, struct_type, 0, node->as.struct_decl.is_public,
                       checker->current_module);
        break;
    }

    case NODE_ENUM_DECL: {
        const char* name = node->as.enum_decl.name;

        if (checker_lookup(checker, name)) {
            check_error(checker, node->line, node->column, "Redefinition of type '%s'", name);
            return;
        }

        Type* enum_type   = type_enum(name);
        int   value_count = node->as.enum_decl.values.count;

        enum_type->as.enm.value_count = value_count;
        enum_type->as.enm.value_names = xmalloc(value_count * sizeof(char*));

        checker_define(checker, name, SYM_TYPE, enum_type, 0, node->as.enum_decl.is_public,
                       checker->current_module);

        // Define enum values as constants
        for (int i = 0; i < value_count; i++) {
            Node* val                        = node->as.enum_decl.values.nodes[i];
            enum_type->as.enm.value_names[i] = xstrdup(val->as.ident.name);
            // Note: enum values are NOT registered in scope - they must be accessed via
            // EnumName::ValueName
        }
        break;
    }

    case NODE_VAR_DECL:
        // Global variable
        check_statement(checker, node);
        break;

    default:
        check_error(checker, node->line, node->column, "Unknown declaration type %d", node->type);
        break;
    }
}

// =============================================================================
// Main entry point
// =============================================================================

int checker_check(Checker* checker, Node* ast) {
    if (!ast || ast->type != NODE_PROGRAM) {
        return 0;
    }

    checker_push_scope(checker); // Global scope

    // First pass: declare all types (structs, enums) for forward references
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Set current module context (NULL for "main", module name for library imports)
        checker->current_module =
            strcmp(mod->as.module.name, "main") == 0 ? NULL : mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL || decl->type == NODE_ENUM_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    // Second pass: check everything else
    // Process library modules (non-main) first, then main module last
    // This ensures library functions are declared before main uses them
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        // Skip main module in this pass
        if (strcmp(mod->as.module.name, "main") == 0)
            continue;
        checker->current_module = mod->as.module.name;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL && decl->type != NODE_ENUM_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    // Third pass: check main module
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        if (strcmp(mod->as.module.name, "main") != 0)
            continue;
        checker->current_module = NULL;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL && decl->type != NODE_ENUM_DECL) {
                check_decl(checker, decl);
            }
        }
    }

    checker->current_module = NULL;
    checker_pop_scope(checker);

    return checker->error_count == 0;
}
