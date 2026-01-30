#include "types.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

// Built-in type singletons
static Type builtin_void = { TYPE_VOID, { { NULL } } };
static Type builtin_bool = { TYPE_BOOL, { { NULL } } };
static Type builtin_int = { TYPE_INT, { { NULL } } };
static Type builtin_int8 = { TYPE_INT8, { { NULL } } };
static Type builtin_int16 = { TYPE_INT16, { { NULL } } };
static Type builtin_int32 = { TYPE_INT32, { { NULL } } };
static Type builtin_uint8 = { TYPE_UINT8, { { NULL } } };
static Type builtin_uint16 = { TYPE_UINT16, { { NULL } } };
static Type builtin_uint32 = { TYPE_UINT32, { { NULL } } };
static Type builtin_float = { TYPE_FLOAT, { { NULL } } };
static Type builtin_char = { TYPE_CHAR, { { NULL } } };
static Type builtin_string = { TYPE_STRING, { { NULL } } };
static Type builtin_error = { TYPE_ERROR, { { NULL } } };

Type *type_void = &builtin_void;
Type *type_bool = &builtin_bool;
Type *type_int = &builtin_int;
Type *type_int8 = &builtin_int8;
Type *type_int16 = &builtin_int16;
Type *type_int32 = &builtin_int32;
Type *type_uint8 = &builtin_uint8;
Type *type_uint16 = &builtin_uint16;
Type *type_uint32 = &builtin_uint32;
Type *type_float = &builtin_float;
Type *type_char = &builtin_char;
Type *type_string = &builtin_string;
Type *type_error = &builtin_error;

// Track allocated types for cleanup
static Type **allocated_types = NULL;
static int allocated_count = 0;
static int allocated_capacity = 0;

static void track_type(Type *type) {
    if (allocated_count >= allocated_capacity) {
        int new_cap = allocated_capacity == 0 ? 64 : allocated_capacity * 2;
        allocated_types = realloc(allocated_types, new_cap * sizeof(Type *));
        allocated_capacity = new_cap;
    }
    allocated_types[allocated_count++] = type;
}

void types_init(void) {
    // Nothing to do - builtins are static
}

void types_cleanup(void) {
    for (int i = 0; i < allocated_count; i++) {
        type_free(allocated_types[i]);
    }
    free(allocated_types);
    allocated_types = NULL;
    allocated_count = 0;
    allocated_capacity = 0;
}

Type *type_new(TypeKind kind) {
    Type *type = calloc(1, sizeof(Type));
    type->kind = kind;
    track_type(type);
    return type;
}

Type *type_pointer(Type *inner) {
    Type *type = type_new(TYPE_POINTER);
    type->as.pointer.inner = inner;
    return type;
}

Type *type_array(Type *elem, int size) {
    Type *type = type_new(TYPE_ARRAY);
    type->as.array.elem = elem;
    type->as.array.size = size;
    return type;
}

Type *type_struct(const char *name) {
    Type *type = type_new(TYPE_STRUCT);
    type->as.struc.name = strdup(name);
    type->as.struc.field_names = NULL;
    type->as.struc.field_types = NULL;
    type->as.struc.field_count = 0;
    return type;
}

Type *type_enum(const char *name) {
    Type *type = type_new(TYPE_ENUM);
    type->as.enm.name = strdup(name);
    type->as.enm.value_names = NULL;
    type->as.enm.value_count = 0;
    return type;
}

Type *type_func(Type **params, int param_count, Type *return_type) {
    Type *type = type_new(TYPE_FUNC);
    type->as.func.param_types = params;
    type->as.func.param_count = param_count;
    type->as.func.return_type = return_type;
    return type;
}

void type_free(Type *type) {
    if (!type) return;
    // Don't free builtins
    if (type == type_void || type == type_bool || type == type_int ||
        type == type_int8 || type == type_int16 || type == type_int32 ||
        type == type_uint8 || type == type_uint16 || type == type_uint32 ||
        type == type_float || type == type_char || type == type_string ||
        type == type_error) {
        return;
    }

    switch (type->kind) {
        case TYPE_STRUCT:
            free(type->as.struc.name);
            for (int i = 0; i < type->as.struc.field_count; i++) {
                free(type->as.struc.field_names[i]);
            }
            free(type->as.struc.field_names);
            free(type->as.struc.field_types);
            break;
        case TYPE_ENUM:
            free(type->as.enm.name);
            for (int i = 0; i < type->as.enm.value_count; i++) {
                free(type->as.enm.value_names[i]);
            }
            free(type->as.enm.value_names);
            break;
        case TYPE_FUNC:
            free(type->as.func.param_types);
            break;
        default:
            break;
    }
    free(type);
}

int type_equals(Type *a, Type *b) {
    if (a == b) return 1;
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;

    switch (a->kind) {
        case TYPE_POINTER:
            return type_equals(a->as.pointer.inner, b->as.pointer.inner);
        case TYPE_ARRAY:
            return a->as.array.size == b->as.array.size &&
                   type_equals(a->as.array.elem, b->as.array.elem);
        case TYPE_STRUCT:
            return strcmp(a->as.struc.name, b->as.struc.name) == 0;
        case TYPE_ENUM:
            return strcmp(a->as.enm.name, b->as.enm.name) == 0;
        case TYPE_FUNC:
            if (a->as.func.param_count != b->as.func.param_count) return 0;
            if (!type_equals(a->as.func.return_type, b->as.func.return_type)) return 0;
            for (int i = 0; i < a->as.func.param_count; i++) {
                if (!type_equals(a->as.func.param_types[i], b->as.func.param_types[i])) {
                    return 0;
                }
            }
            return 1;
        default:
            return 1;  // For primitives, kind equality is enough
    }
}

int type_is_integer(Type *type) {
    if (!type) return 0;
    return type->kind == TYPE_INT || type->kind == TYPE_INT8 ||
           type->kind == TYPE_INT16 || type->kind == TYPE_INT32 ||
           type->kind == TYPE_UINT8 || type->kind == TYPE_UINT16 ||
           type->kind == TYPE_UINT32;
}

int type_is_signed_integer(Type *type) {
    if (!type) return 0;
    return type->kind == TYPE_INT || type->kind == TYPE_INT8 ||
           type->kind == TYPE_INT16 || type->kind == TYPE_INT32;
}

int type_is_unsigned_integer(Type *type) {
    if (!type) return 0;
    return type->kind == TYPE_UINT8 || type->kind == TYPE_UINT16 ||
           type->kind == TYPE_UINT32;
}

int type_assignable(Type *target, Type *value) {
    if (type_equals(target, value)) return 1;
    if (target->kind == TYPE_ERROR || value->kind == TYPE_ERROR) return 1;

    // Any integer -> float promotion
    if (target->kind == TYPE_FLOAT && type_is_integer(value)) return 1;

    // int (64-bit) can be assigned to any integer type (implicit narrowing)
    // This allows integer literals to be assigned to smaller types
    if (type_is_integer(target) && value->kind == TYPE_INT) return 1;

    // null can be assigned to pointers
    if (target->kind == TYPE_POINTER && value->kind == TYPE_POINTER &&
        value->as.pointer.inner == NULL) {
        return 1;
    }

    // Array to pointer decay
    if (target->kind == TYPE_POINTER && value->kind == TYPE_ARRAY) {
        return type_assignable(target->as.pointer.inner, value->as.array.elem);
    }

    return 0;
}

static char type_name_buf[256];

const char *type_name(Type *type) {
    if (!type) return "(null)";

    switch (type->kind) {
        case TYPE_VOID: return "void";
        case TYPE_BOOL: return "bool";
        case TYPE_INT: return "int";
        case TYPE_INT8: return "int8";
        case TYPE_INT16: return "int16";
        case TYPE_INT32: return "int32";
        case TYPE_UINT8: return "uint8";
        case TYPE_UINT16: return "uint16";
        case TYPE_UINT32: return "uint32";
        case TYPE_FLOAT: return "float";
        case TYPE_CHAR: return "char";
        case TYPE_STRING: return "string";
        case TYPE_ERROR: return "<error>";
        case TYPE_POINTER:
            snprintf(type_name_buf, sizeof(type_name_buf), "*%s",
                     type_name(type->as.pointer.inner));
            return type_name_buf;
        case TYPE_ARRAY:
            if (type->as.array.size >= 0) {
                snprintf(type_name_buf, sizeof(type_name_buf), "[%d]%s",
                         type->as.array.size, type_name(type->as.array.elem));
            } else {
                snprintf(type_name_buf, sizeof(type_name_buf), "[]%s",
                         type_name(type->as.array.elem));
            }
            return type_name_buf;
        case TYPE_STRUCT:
            return type->as.struc.name;
        case TYPE_ENUM:
            return type->as.enm.name;
        case TYPE_FUNC:
            snprintf(type_name_buf, sizeof(type_name_buf), "func(...): %s",
                     type_name(type->as.func.return_type));
            return type_name_buf;
    }
    return "<unknown>";
}

void typelist_init(TypeList *list) {
    list->types = NULL;
    list->count = 0;
    list->capacity = 0;
}

void typelist_push(TypeList *list, Type *type) {
    if (list->count >= list->capacity) {
        int new_cap = list->capacity == 0 ? 8 : list->capacity * 2;
        list->types = realloc(list->types, new_cap * sizeof(Type *));
        list->capacity = new_cap;
    }
    list->types[list->count++] = type;
}

void typelist_free(TypeList *list) {
    free(list->types);
    list->types = NULL;
    list->count = 0;
    list->capacity = 0;
}
