#ifndef WHIST_TYPES_H
#define WHIST_TYPES_H

#include <stddef.h>

typedef enum {
    TYPE_VOID,
    TYPE_BOOL,
    TYPE_INT64, // 64-bit signed (default)
    TYPE_INT8,
    TYPE_INT16,
    TYPE_INT32,
    TYPE_UINT64,
    TYPE_UINT8,
    TYPE_UINT16,
    TYPE_UINT32,
    TYPE_F32,
    TYPE_F64,
    TYPE_CHAR,
    TYPE_STRING,
    TYPE_VOIDPTR,
    TYPE_ARRAY,
    TYPE_SPAN,
    TYPE_VEC,
    TYPE_STRINGBUILDER,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_TRAIT,
    TYPE_FUNC,
    TYPE_TUPLE,
    TYPE_NULL,          // null reference type
    TYPE_GENERIC_PARAM, // Generic type parameter (T, K, V)
    TYPE_ERROR,         // Used for error recovery
} TypeKind;

typedef struct Type     Type;
typedef struct TypeList TypeList;

struct TypeList {
    Type** types;
    int    count;
    int    capacity;
};

struct Type {
    TypeKind kind;
    union {
        struct {
            Type* unused;
        } builtin;
        // Array: [size]elem
        struct {
            Type* elem;
            int   size; // -1 for dynamic/unknown size
        } array;

        // Span: Span<T>
        struct {
            Type* elem; // Element type T
        } span;

        // Vec: Vec<T>
        struct {
            Type* elem; // Element type T
        } vec;

        // Struct
        struct {
            char*  name;
            char** field_names;
            Type** field_types;
            int*   field_is_const;
            int    field_count;
            // Methods
            char** method_names;
            Type** method_types;    // Function types for methods
            int*   method_is_const; // Whether each method is const
            int    method_count;
            int    has_drop;      // 1 if type implements Drop trait
            int    has_rc_fields; // 1 if any field is TYPE_STRUCT (RC pointer)
            int    has_eq;        // 1 if type implements Eq trait
        } struc;

        // Enum
        struct {
            char*   name;
            char**  value_names;
            int     value_count;
            int     has_data;            // 1 if any variant carries payload
            int     has_rc_fields;       // 1 if any payload carries RC-managed pointers
            Type*** variant_types;       // [variant_idx] -> Type*[] (NULL = no payload)
            int*    variant_type_counts; // [variant_idx] -> field count
            // Methods
            char** method_names;
            Type** method_types;    // Function types for methods
            int*   method_is_const; // Whether each method is const
            int    method_count;
        } enm;

        // Trait
        struct {
            char*  name;
            char** method_names;
            Type** method_types;    // Function types (signatures without receiver)
            int*   method_is_const; // Whether each method requires const receiver
            int    method_count;
        } trait;

        // Function
        struct {
            Type** param_types;
            int    param_count;
            Type*  return_type;
            int    is_varargs;
        } func;

        // Tuple
        struct {
            Type** elem_types;
            int    elem_count;
        } tuple;

        // Generic type parameter (T, K, V, etc.)
        struct {
            char* name;
        } generic_param;
    } as;
};

// Built-in types
extern Type* type_void;
extern Type* type_bool;
extern Type* type_int64;
extern Type* type_int8;
extern Type* type_int16;
extern Type* type_int32;
extern Type* type_uint64;
extern Type* type_uint8;
extern Type* type_uint16;
extern Type* type_uint32;
extern Type* type_f32;
extern Type* type_f64;
extern Type* type_char;
extern Type* type_string;
extern Type* type_voidptr;
extern Type* type_stringbuilder;
extern Type* type_error;
extern Type* type_null;

// Type operations
void types_init(void);
void types_cleanup(void);

Type* type_new(TypeKind kind);
Type* type_array(Type* elem, int size);
Type* type_struct(const char* name);
Type* type_enum(const char* name);
Type* type_trait(const char* name);
Type* type_func(Type** params, int param_count, Type* return_type, int is_varargs);
Type* type_tuple(Type** elems, int count);
Type* type_generic_param(const char* name);
Type* type_span(Type* elem);
Type* type_vec(Type* elem);

// Type name mangling for C identifiers (no angle brackets or special chars)
const char* type_mangle_name(Type* type);
char*       type_mangle_generic(const char* base, Type** args, int count);

void        type_free(Type* type);
int         type_equals(Type* a, Type* b);
int         type_assignable(Type* target, Type* value); // Can value be assigned to target?
int         type_is_integer(Type* type);                // Is this any integer type?
int         type_is_rc_managed(Type* type);             // Is this an RC-managed type as a field?
int         type_is_signed_integer(Type* type);         // Is this a signed integer type?
int         type_is_unsigned_integer(Type* type);       // Is this an unsigned integer type?
int         type_supports_vec_contains(Type* type);     // Can Vec<T>.contains compare this type?
int         type_supports_vec_sort(Type* type);         // Is this type sortable by Vec<T>.sort?
const char* type_name(Type* type);

// Builtin type lookup utilities
Type*       type_builtin_from_name(const char* name); // Returns builtin Type* or NULL
const char* type_c_name(const char* name);            // Returns C type string or NULL
int         type_is_builtin_name(const char* name);   // Is this a builtin type name?

// TypeList operations
void typelist_init(TypeList* list);
void typelist_push(TypeList* list, Type* type);
void typelist_free(TypeList* list);

#endif
