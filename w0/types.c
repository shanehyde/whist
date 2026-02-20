#include "types.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"

// Built-in type singletons
static Type builtin_void          = {TYPE_VOID, {{NULL}}};
static Type builtin_bool          = {TYPE_BOOL, {{NULL}}};
static Type builtin_int64         = {TYPE_INT64, {{NULL}}};
static Type builtin_int8          = {TYPE_INT8, {{NULL}}};
static Type builtin_int16         = {TYPE_INT16, {{NULL}}};
static Type builtin_int32         = {TYPE_INT32, {{NULL}}};
static Type builtin_uint64        = {TYPE_UINT64, {{NULL}}};
static Type builtin_uint8         = {TYPE_UINT8, {{NULL}}};
static Type builtin_uint16        = {TYPE_UINT16, {{NULL}}};
static Type builtin_uint32        = {TYPE_UINT32, {{NULL}}};
static Type builtin_f32           = {TYPE_F32, {{NULL}}};
static Type builtin_f64           = {TYPE_F64, {{NULL}}};
static Type builtin_char          = {TYPE_CHAR, {{NULL}}};
static Type builtin_string        = {TYPE_STRING, {{NULL}}};
static Type builtin_voidptr       = {TYPE_VOIDPTR, {{NULL}}};
static Type builtin_stringbuilder = {TYPE_STRINGBUILDER, {{NULL}}};
static Type builtin_error         = {TYPE_ERROR, {{NULL}}};
static Type builtin_null          = {TYPE_NULL, {{NULL}}};

Type* type_void          = &builtin_void;
Type* type_bool          = &builtin_bool;
Type* type_int64         = &builtin_int64;
Type* type_int8          = &builtin_int8;
Type* type_int16         = &builtin_int16;
Type* type_int32         = &builtin_int32;
Type* type_uint64        = &builtin_uint64;
Type* type_uint8         = &builtin_uint8;
Type* type_uint16        = &builtin_uint16;
Type* type_uint32        = &builtin_uint32;
Type* type_f32           = &builtin_f32;
Type* type_f64           = &builtin_f64;
Type* type_char          = &builtin_char;
Type* type_string        = &builtin_string;
Type* type_voidptr       = &builtin_voidptr;
Type* type_stringbuilder = &builtin_stringbuilder;
Type* type_error         = &builtin_error;
Type* type_null          = &builtin_null;

// Track allocated types for cleanup
static Type** allocated_types    = NULL;
static int    allocated_count    = 0;
static int    allocated_capacity = 0;

static void track_type(Type* type) {
    VEC_GROW_N(allocated_types, allocated_count, allocated_capacity, 64);
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
    allocated_types    = NULL;
    allocated_count    = 0;
    allocated_capacity = 0;
}

Type* type_new(TypeKind kind) {
    Type* type = xcalloc(1, sizeof(Type));
    type->kind = kind;
    track_type(type);
    return type;
}

Type* type_array(Type* elem, int size) {
    Type* type          = type_new(TYPE_ARRAY);
    type->as.array.elem = elem;
    type->as.array.size = size;
    return type;
}

Type* type_struct(const char* name) {
    Type* type                      = type_new(TYPE_STRUCT);
    type->as.struc.name             = xstrdup(name);
    type->as.struc.base_name        = type->as.struc.name; // same allocation for non-generic
    type->as.struc.field_names      = NULL;
    type->as.struc.field_types      = NULL;
    type->as.struc.field_is_const   = NULL;
    type->as.struc.field_is_private = NULL;
    type->as.struc.field_count      = 0;
    type->as.struc.method_names     = NULL;
    type->as.struc.method_types     = NULL;
    type->as.struc.method_is_const  = NULL;
    type->as.struc.method_count     = 0;
    return type;
}

Type* type_enum(const char* name) {
    Type* type                       = type_new(TYPE_ENUM);
    type->as.enm.name                = xstrdup(name);
    type->as.enm.value_names         = NULL;
    type->as.enm.value_count         = 0;
    type->as.enm.has_data            = 0;
    type->as.enm.has_rc_fields       = 0;
    type->as.enm.variant_types       = NULL;
    type->as.enm.variant_type_counts = NULL;
    type->as.enm.method_names        = NULL;
    type->as.enm.method_types        = NULL;
    type->as.enm.method_is_const     = NULL;
    type->as.enm.method_count        = 0;
    return type;
}

Type* type_trait(const char* name) {
    Type* type                     = type_new(TYPE_TRAIT);
    type->as.trait.name            = xstrdup(name);
    type->as.trait.method_names    = NULL;
    type->as.trait.method_types    = NULL;
    type->as.trait.method_is_const = NULL;
    type->as.trait.method_count    = 0;
    return type;
}

Type* type_func(Type** params, int param_count, Type* return_type, int is_varargs) {
    Type* type                = type_new(TYPE_FUNC);
    type->as.func.param_types = params;
    type->as.func.param_count = param_count;
    type->as.func.return_type = return_type;
    type->as.func.is_varargs  = is_varargs;
    return type;
}

Type* type_tuple(Type** elems, int count) {
    Type* type                = type_new(TYPE_TUPLE);
    type->as.tuple.elem_types = elems;
    type->as.tuple.elem_count = count;
    return type;
}

Type* type_generic_param(const char* name) {
    Type* type                  = type_new(TYPE_GENERIC_PARAM);
    type->as.generic_param.name = xstrdup(name);
    return type;
}

Type* type_span(Type* elem) {
    Type* type         = type_new(TYPE_SPAN);
    type->as.span.elem = elem;
    return type;
}

Type* type_vec(Type* elem) {
    Type* type        = type_new(TYPE_VEC);
    type->as.vec.elem = elem;
    return type;
}

void type_free(Type* type) {
    if (!type)
        return;
    // Don't free builtins
    if (type == type_void || type == type_bool || type == type_int64 || type == type_int8 ||
        type == type_int16 || type == type_int32 || type == type_uint64 || type == type_uint8 ||
        type == type_uint16 || type == type_uint32 || type == type_f32 || type == type_f64 ||
        type == type_char || type == type_string || type == type_voidptr ||
        type == type_stringbuilder || type == type_error || type == type_null) {
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
        free(type->as.struc.field_is_const);
        for (int i = 0; i < type->as.struc.method_count; i++) {
            free(type->as.struc.method_names[i]);
        }
        free(type->as.struc.method_names);
        free(type->as.struc.method_types);
        free(type->as.struc.method_is_const);
        break;
    case TYPE_ENUM:
        free(type->as.enm.name);
        for (int i = 0; i < type->as.enm.value_count; i++) {
            free(type->as.enm.value_names[i]);
        }
        free(type->as.enm.value_names);
        if (type->as.enm.variant_types) {
            for (int i = 0; i < type->as.enm.value_count; i++) {
                free(type->as.enm.variant_types[i]);
            }
            free(type->as.enm.variant_types);
        }
        free(type->as.enm.variant_type_counts);
        for (int i = 0; i < type->as.enm.method_count; i++) {
            free(type->as.enm.method_names[i]);
        }
        free(type->as.enm.method_names);
        free(type->as.enm.method_types);
        free(type->as.enm.method_is_const);
        break;
    case TYPE_TRAIT:
        free(type->as.trait.name);
        for (int i = 0; i < type->as.trait.method_count; i++) {
            free(type->as.trait.method_names[i]);
        }
        free(type->as.trait.method_names);
        free(type->as.trait.method_types);
        free(type->as.trait.method_is_const);
        break;
    case TYPE_FUNC:
        free(type->as.func.param_types);
        break;
    case TYPE_TUPLE:
        free(type->as.tuple.elem_types);
        break;
    case TYPE_GENERIC_PARAM:
        free(type->as.generic_param.name);
        break;
    default:
        break;
    }
    free(type);
}

int type_equals(Type* a, Type* b) {
    if (a == b)
        return 1;
    if (!a || !b)
        return 0;
    if (a->kind != b->kind)
        return 0;

    switch (a->kind) {
    case TYPE_ARRAY:
        return a->as.array.size == b->as.array.size &&
               type_equals(a->as.array.elem, b->as.array.elem);
    case TYPE_SPAN:
        return type_equals(a->as.span.elem, b->as.span.elem);
    case TYPE_VEC:
        return type_equals(a->as.vec.elem, b->as.vec.elem);
    case TYPE_STRUCT:
        return strcmp(a->as.struc.name, b->as.struc.name) == 0;
    case TYPE_ENUM:
        return strcmp(a->as.enm.name, b->as.enm.name) == 0;
    case TYPE_TRAIT:
        return strcmp(a->as.trait.name, b->as.trait.name) == 0;
    case TYPE_FUNC:
        if (a->as.func.param_count != b->as.func.param_count)
            return 0;
        if (a->as.func.is_varargs != b->as.func.is_varargs)
            return 0;
        if (!type_equals(a->as.func.return_type, b->as.func.return_type))
            return 0;
        for (int i = 0; i < a->as.func.param_count; i++) {
            if (!type_equals(a->as.func.param_types[i], b->as.func.param_types[i])) {
                return 0;
            }
        }
        return 1;
    case TYPE_TUPLE:
        if (a->as.tuple.elem_count != b->as.tuple.elem_count)
            return 0;
        for (int i = 0; i < a->as.tuple.elem_count; i++) {
            if (!type_equals(a->as.tuple.elem_types[i], b->as.tuple.elem_types[i])) {
                return 0;
            }
        }
        return 1;
    case TYPE_GENERIC_PARAM:
        return strcmp(a->as.generic_param.name, b->as.generic_param.name) == 0;
    default:
        return 1; // For primitives, kind equality is enough
    }
}

int type_is_integer(Type* type) {
    if (!type)
        return 0;
    return type->kind == TYPE_INT64 || type->kind == TYPE_INT8 || type->kind == TYPE_INT16 ||
           type->kind == TYPE_INT32 || type->kind == TYPE_UINT64 || type->kind == TYPE_UINT8 ||
           type->kind == TYPE_UINT16 || type->kind == TYPE_UINT32 || type->kind == TYPE_CHAR;
}

int type_is_signed_integer(Type* type) {
    if (!type)
        return 0;
    return type->kind == TYPE_INT64 || type->kind == TYPE_INT8 || type->kind == TYPE_INT16 ||
           type->kind == TYPE_INT32;
}

int type_is_unsigned_integer(Type* type) {
    if (!type)
        return 0;
    return type->kind == TYPE_UINT64 || type->kind == TYPE_UINT8 || type->kind == TYPE_UINT16 ||
           type->kind == TYPE_UINT32;
}

int type_supports_vec_contains(Type* type) {
    if (!type)
        return 0;

    switch (type->kind) {
    case TYPE_BOOL:
    case TYPE_INT64:
    case TYPE_INT8:
    case TYPE_INT16:
    case TYPE_INT32:
    case TYPE_UINT64:
    case TYPE_UINT8:
    case TYPE_UINT16:
    case TYPE_UINT32:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_CHAR:
    case TYPE_STRING:
    case TYPE_VOIDPTR:
        return 1;
    case TYPE_ENUM:
        return !type->as.enm.has_data;
    default:
        return 0;
    }
}

int type_supports_vec_sort(Type* type) {
    if (!type)
        return 0;

    switch (type->kind) {
    case TYPE_BOOL:
    case TYPE_INT64:
    case TYPE_INT8:
    case TYPE_INT16:
    case TYPE_INT32:
    case TYPE_UINT64:
    case TYPE_UINT8:
    case TYPE_UINT16:
    case TYPE_UINT32:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_CHAR:
    case TYPE_STRING:
        return 1;
    default:
        return 0;
    }
}

int type_supports_equality(Type* type) {
    if (!type)
        return 0;
    switch (type->kind) {
    case TYPE_BOOL:
    case TYPE_INT64:
    case TYPE_INT8:
    case TYPE_INT16:
    case TYPE_INT32:
    case TYPE_UINT64:
    case TYPE_UINT8:
    case TYPE_UINT16:
    case TYPE_UINT32:
    case TYPE_F32:
    case TYPE_F64:
    case TYPE_CHAR:
    case TYPE_STRING:
    case TYPE_VOIDPTR:
        return 1;
    case TYPE_ENUM:
        return !type->as.enm.has_data; // non-data enums only (for now)
    case TYPE_STRUCT:
        return type->as.struc.has_eq;
    case TYPE_VEC:
        return type_supports_equality(type->as.vec.elem); // recursive
    default:
        return 0;
    }
}

// Is this type RC-managed when held as a struct field or enum variant payload?
// Structs and Vecs are always heap-allocated (RC pointers). Enums are RC-managed
// only if they carry RC-managed payloads themselves.
int type_is_rc_managed(Type* type) {
    if (!type)
        return 0;
    if (type->kind == TYPE_STRUCT || type->kind == TYPE_VEC || type->kind == TYPE_STRINGBUILDER ||
        type->kind == TYPE_STRING)
        return 1;
    if (type->kind == TYPE_ENUM && type->as.enm.has_rc_fields)
        return 1;
    return 0;
}

int type_assignable(Type* target, Type* value) {
    if (type_equals(target, value))
        return 1;
    if (target->kind == TYPE_ERROR || value->kind == TYPE_ERROR)
        return 1;

    // Any integer -> float promotion
    if ((target->kind == TYPE_F32 || target->kind == TYPE_F64) && type_is_integer(value))
        return 1;

    // f32 -> f64 promotion
    if (target->kind == TYPE_F64 && value->kind == TYPE_F32)
        return 1;

    // i64 can be assigned to any integer type (implicit narrowing)
    // This allows integer literals to be assigned to smaller types
    if (type_is_integer(target) && value->kind == TYPE_INT64)
        return 1;

    // null can be assigned to struct references
    if (target->kind == TYPE_STRUCT && value->kind == TYPE_NULL) {
        return 1;
    }

    // null can be assigned to vec references
    if (target->kind == TYPE_VEC && value->kind == TYPE_NULL) {
        return 1;
    }

    // null can be assigned to StringBuilder references
    if (target->kind == TYPE_STRINGBUILDER && value->kind == TYPE_NULL) {
        return 1;
    }

    // null can be assigned to voidptr
    if (target->kind == TYPE_VOIDPTR && value->kind == TYPE_NULL) {
        return 1;
    }

    // null can be assigned to function pointers
    if (target->kind == TYPE_FUNC && value->kind == TYPE_NULL) {
        return 1;
    }

    return 0;
}

// Ring buffer for type_name() results. Multiple concurrent calls (e.g. in the same
// printf argument list) each get their own buffer slot, avoiding aliasing bugs.
#define TYPE_NAME_BUFS 8
static char type_name_bufs[TYPE_NAME_BUFS][256];
static int  type_name_idx = 0;

static char* next_type_name_buf(void) {
    char* buf     = type_name_bufs[type_name_idx];
    type_name_idx = (type_name_idx + 1) % TYPE_NAME_BUFS;
    return buf;
}

const char* type_name(Type* type) {
    if (!type)
        return "(null)";

    switch (type->kind) {
    case TYPE_VOID:
        return "void";
    case TYPE_BOOL:
        return "bool";
    case TYPE_INT64:
        return "i64";
    case TYPE_INT8:
        return "i8";
    case TYPE_INT16:
        return "i16";
    case TYPE_INT32:
        return "i32";
    case TYPE_UINT64:
        return "u64";
    case TYPE_UINT8:
        return "u8";
    case TYPE_UINT16:
        return "u16";
    case TYPE_UINT32:
        return "u32";
    case TYPE_F32:
        return "f32";
    case TYPE_F64:
        return "f64";
    case TYPE_CHAR:
        return "char";
    case TYPE_STRING:
        return "string";
    case TYPE_VOIDPTR:
        return "voidptr";
    case TYPE_STRINGBUILDER:
        return "StringBuilder";
    case TYPE_ERROR:
        return "<error>";
    case TYPE_NULL:
        return "null";
    case TYPE_ARRAY: {
        char* buf = next_type_name_buf();
        if (type->as.array.size >= 0) {
            snprintf(buf, 256, "[%d]%s", type->as.array.size, type_name(type->as.array.elem));
        } else {
            snprintf(buf, 256, "[]%s", type_name(type->as.array.elem));
        }
        return buf;
    }
    case TYPE_SPAN: {
        char* buf = next_type_name_buf();
        snprintf(buf, 256, "Span<%s>", type_name(type->as.span.elem));
        return buf;
    }
    case TYPE_VEC: {
        char* buf = next_type_name_buf();
        snprintf(buf, 256, "Vec<%s>", type_name(type->as.vec.elem));
        return buf;
    }
    case TYPE_STRUCT:
        return type->as.struc.name;
    case TYPE_ENUM:
        return type->as.enm.name;
    case TYPE_TRAIT:
        return type->as.trait.name;
    case TYPE_FUNC: {
        char* buf = next_type_name_buf();
        int   n   = snprintf(buf, 256, "func(");
        for (int i = 0; i < type->as.func.param_count && n < 255; i++) {
            if (i > 0)
                n += snprintf(buf + n, 256 - n, ", ");
            n += snprintf(buf + n, 256 - n, "%s", type_name(type->as.func.param_types[i]));
        }
        n += snprintf(buf + n, 256 - n, ")");
        if (type->as.func.return_type && type->as.func.return_type->kind != TYPE_VOID) {
            snprintf(buf + n, 256 - n, " -> %s", type_name(type->as.func.return_type));
        }
        return buf;
    }
    case TYPE_TUPLE: {
        char* buf = next_type_name_buf();
        char* p   = buf;
        char* end = buf + 256 - 1;
        *p++      = '(';
        for (int i = 0; i < type->as.tuple.elem_count && p < end; i++) {
            if (i > 0) {
                if (p + 2 < end) {
                    *p++ = ',';
                    *p++ = ' ';
                }
            }
            const char* elem_name = type_name(type->as.tuple.elem_types[i]);
            while (*elem_name && p < end) {
                *p++ = *elem_name++;
            }
        }
        if (p < end)
            *p++ = ')';
        *p = '\0';
        return buf;
    }
    case TYPE_GENERIC_PARAM:
        return type->as.generic_param.name;
    }
    return "<unknown>";
}

// Builtin type lookup table
static struct {
    const char* whist_name;
    Type**      type_ptr;
    const char* c_name;
} builtin_types[] = {
    {"void", &type_void, "void"},
    {"bool", &type_bool, "bool"},
    {"i64", &type_int64, "int64_t"},
    {"i8", &type_int8, "int8_t"},
    {"i16", &type_int16, "int16_t"},
    {"i32", &type_int32, "int32_t"},
    {"u64", &type_uint64, "uint64_t"},
    {"u8", &type_uint8, "uint8_t"},
    {"u16", &type_uint16, "uint16_t"},
    {"u32", &type_uint32, "uint32_t"},
    {"f32", &type_f32, "float"},
    {"f64", &type_f64, "double"},
    {"char", &type_char, "char"},
    {"string", &type_string, "const char*"},
    {"voidptr", &type_voidptr, "void*"},
    {"StringBuilder", &type_stringbuilder, "__StringBuilder*"},
    {NULL, NULL, NULL},
};

Type* type_builtin_from_name(const char* name) {
    for (int i = 0; builtin_types[i].whist_name; i++) {
        if (strcmp(name, builtin_types[i].whist_name) == 0) {
            return *builtin_types[i].type_ptr;
        }
    }
    return NULL;
}

const char* type_c_name(const char* name) {
    for (int i = 0; builtin_types[i].whist_name; i++) {
        if (strcmp(name, builtin_types[i].whist_name) == 0) {
            return builtin_types[i].c_name;
        }
    }
    return NULL;
}

int type_is_builtin_name(const char* name) {
    return type_builtin_from_name(name) != NULL;
}

// Buffer for mangled span names
#define TYPE_MANGLE_BUFS 8
static char type_mangle_bufs[TYPE_MANGLE_BUFS][256];
static int  type_mangle_idx = 0;

static char* next_type_mangle_buf(void) {
    char* buf       = type_mangle_bufs[type_mangle_idx];
    type_mangle_idx = (type_mangle_idx + 1) % TYPE_MANGLE_BUFS;
    return buf;
}

// Get a C-identifier-safe name for a type (no angle brackets or special chars)
const char* type_mangle_name(Type* type) {
    if (!type)
        return "void";

    switch (type->kind) {
    case TYPE_VOID:
        return "void";
    case TYPE_BOOL:
        return "bool";
    case TYPE_INT64:
        return "i64";
    case TYPE_INT8:
        return "i8";
    case TYPE_INT16:
        return "i16";
    case TYPE_INT32:
        return "i32";
    case TYPE_UINT64:
        return "u64";
    case TYPE_UINT8:
        return "u8";
    case TYPE_UINT16:
        return "u16";
    case TYPE_UINT32:
        return "u32";
    case TYPE_F32:
        return "f32";
    case TYPE_F64:
        return "f64";
    case TYPE_CHAR:
        return "char";
    case TYPE_STRING:
        return "string";
    case TYPE_VOIDPTR:
        return "voidptr";
    case TYPE_STRINGBUILDER:
        return "StringBuilder";
    case TYPE_SPAN: {
        char* buf = next_type_mangle_buf();
        snprintf(buf, 256, "Span_%s", type_mangle_name(type->as.span.elem));
        return buf;
    }
    case TYPE_VEC: {
        char* buf = next_type_mangle_buf();
        snprintf(buf, 256, "Vec_%s", type_mangle_name(type->as.vec.elem));
        return buf;
    }
    case TYPE_STRUCT:
        return type->as.struc.name;
    case TYPE_ENUM:
        return type->as.enm.name;
    case TYPE_TRAIT:
        return type->as.trait.name;
    case TYPE_GENERIC_PARAM:
        return type->as.generic_param.name;
    case TYPE_ARRAY: {
        char* buf = next_type_mangle_buf();
        snprintf(buf, 256, "arr%d_%s", type->as.array.size, type_mangle_name(type->as.array.elem));
        return buf;
    }
    case TYPE_TUPLE: {
        char* buf = next_type_mangle_buf();
        int   pos = snprintf(buf, 256, "tup");
        for (int i = 0; i < type->as.tuple.elem_count && pos < 255; i++) {
            pos += snprintf(buf + pos, 256 - pos, "_%s",
                            type_mangle_name(type->as.tuple.elem_types[i]));
        }
        return buf;
    }
    case TYPE_FUNC: {
        char* buf = next_type_mangle_buf();
        int   pos = snprintf(buf, 256, "fn");
        for (int i = 0; i < type->as.func.param_count && pos < 255; i++) {
            pos += snprintf(buf + pos, 256 - pos, "_%s",
                            type_mangle_name(type->as.func.param_types[i]));
        }
        if (type->as.func.return_type) {
            pos += snprintf(buf + pos, 256 - pos, "_r_%s",
                            type_mangle_name(type->as.func.return_type));
        }
        return buf;
    }
    default:
        return "unknown";
    }
}

char* type_mangle_generic(const char* base, Type** args, int count) {
    // Calculate required buffer size
    size_t len = strlen(base);
    for (int i = 0; i < count; i++) {
        len += 1 + strlen(type_mangle_name(args[i])); // underscore + type name
    }

    char* result = xmalloc(len + 1);
    char* p      = result;

    // Copy base name
    strcpy(p, base);
    p += strlen(base);

    // Append each type argument with underscore
    for (int i = 0; i < count; i++) {
        *p++              = '_';
        const char* tname = type_mangle_name(args[i]);
        strcpy(p, tname);
        p += strlen(tname);
    }

    *p = '\0';
    return result;
}

void typelist_init(TypeList* list) {
    list->types    = NULL;
    list->count    = 0;
    list->capacity = 0;
}

void typelist_push(TypeList* list, Type* type) {
    VEC_GROW(list->types, list->count, list->capacity);
    list->types[list->count++] = type;
}

void typelist_free(TypeList* list) {
    free(list->types);
    list->types    = NULL;
    list->count    = 0;
    list->capacity = 0;
}
