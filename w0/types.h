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
    TYPE_POINTER,
    TYPE_ARRAY,
    TYPE_STRUCT,
    TYPE_ENUM,
    TYPE_FUNC,
    TYPE_ERROR, // Used for error recovery
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
        // Pointer: *inner
        struct {
            Type* inner;
        } pointer;

        // Array: [size]elem
        struct {
            Type* elem;
            int   size; // -1 for dynamic/unknown size
        } array;

        // Struct
        struct {
            char*  name;
            char** field_names;
            Type** field_types;
            int    field_count;
            // Methods
            char** method_names;
            Type** method_types;    // Function types for methods
            int*   method_is_const; // Whether each method is const
            int    method_count;
        } struc;

        // Enum
        struct {
            char*  name;
            char** value_names;
            int    value_count;
        } enm;

        // Function
        struct {
            Type** param_types;
            int    param_count;
            Type*  return_type;
        } func;
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
extern Type* type_error;

// Type operations
void types_init(void);
void types_cleanup(void);

Type* type_new(TypeKind kind);
Type* type_pointer(Type* inner);
Type* type_array(Type* elem, int size);
Type* type_struct(const char* name);
Type* type_enum(const char* name);
Type* type_func(Type** params, int param_count, Type* return_type);

void        type_free(Type* type);
int         type_equals(Type* a, Type* b);
int         type_assignable(Type* target, Type* value); // Can value be assigned to target?
int         type_is_integer(Type* type);                // Is this any integer type?
int         type_is_signed_integer(Type* type);         // Is this a signed integer type?
int         type_is_unsigned_integer(Type* type);       // Is this an unsigned integer type?
const char* type_name(Type* type);

// TypeList operations
void typelist_init(TypeList* list);
void typelist_push(TypeList* list, Type* type);
void typelist_free(TypeList* list);

#endif
