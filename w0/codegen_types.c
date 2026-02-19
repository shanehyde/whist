#include "codegen_types.h"

#include <stdlib.h>
#include <string.h>

#include "codegen_internal.h"
#include "types.h"

// Check if a name is a registered enum type in the codegen context
int is_enum_type_name(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enums.count; i++) {
        if (strcmp(gen->enums.names[i], name) == 0)
            return 1;
    }
    return 0;
}

// Return the index of an enum name in the registered enum list, or -1 if not found
int enum_index(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->enums.count; i++) {
        if (strcmp(gen->enums.names[i], name) == 0)
            return i;
    }
    return -1;
}

// Check if a named enum type has RC-managed fields
int enum_has_rc_fields(CodeGen* gen, const char* name) {
    int idx = enum_index(gen, name);
    if (idx < 0 || !gen->enums.has_rc_fields)
        return 0;
    return gen->enums.has_rc_fields[idx];
}

// Check if a named data enum supports equality comparison
int enum_has_eq(CodeGen* gen, const char* name) {
    int idx = enum_index(gen, name);
    if (idx < 0 || !gen->enums.has_eq)
        return 0;
    return gen->enums.has_eq[idx];
}

// Resolve a type node through aliases. If the node is a NODE_IDENT
// that names a type alias, return the alias target node instead.
Node* resolve_alias(CodeGen* gen, Node* type_node) {
    if (!type_node || type_node->type != NODE_IDENT)
        return type_node;
    for (int i = 0; i < gen->aliases.type_count; i++) {
        if (strcmp(gen->aliases.types[i], type_node->as.ident.name) == 0) {
            return gen->aliases.type_targets[i];
        }
    }
    return type_node;
}

// Check if a type node represents a struct (user-defined) type
int is_struct_type(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return 0;
    type_node = resolve_alias(gen, type_node);
    // Generic types (Box<i64>) are always struct types, except Span<T>, Vec<T>, and generic enums
    if (type_node->type == NODE_GENERIC_TYPE) {
        if (strcmp(type_node->as.generic_type.base_name, "Span") == 0) {
            return 0;
        }
        if (strcmp(type_node->as.generic_type.base_name, "Vec") == 0) {
            return 0;
        }
        // Check if the mangled name is a registered enum
        char* mangled = build_mangled_name_from_generic_node(gen, type_node);
        int   is_enum = is_enum_type_name(gen, mangled);
        free(mangled);
        if (is_enum)
            return 0;
        return 1;
    }
    if (type_node->type != NODE_IDENT)
        return 0;
    if (is_enum_type_name(gen, type_node->as.ident.name))
        return 0;
    return !type_is_builtin_name(type_node->as.ident.name);
}

// Check if a type node represents an RC-managed type (struct, Vec, or enum with RC fields)
int type_node_has_rc(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return 0;
    type_node = resolve_alias(gen, type_node);
    if (type_node->type == NODE_GENERIC_TYPE) {
        if (strcmp(type_node->as.generic_type.base_name, "Span") == 0)
            return 0;
        if (strcmp(type_node->as.generic_type.base_name, "Vec") == 0)
            return 1; // Vec is always RC-managed
        // Check if this is a generic enum
        char* mangled = build_mangled_name_from_generic_node(gen, type_node);
        if (is_enum_type_name(gen, mangled)) {
            int has_rc = enum_has_rc_fields(gen, mangled);
            free(mangled);
            return has_rc;
        }
        free(mangled);
        return 1; // Generic struct — always RC
    }
    if (type_node->type != NODE_IDENT)
        return 0;

    const char* name = type_node->as.ident.name;

    // Handle type parameter substitution
    Type* resolved = subst_lookup(gen, name);
    if (resolved) {
        if (resolved->kind == TYPE_ENUM)
            return resolved->as.enm.has_rc_fields;
        if (resolved->kind == TYPE_STRUCT || resolved->kind == TYPE_STRING ||
            resolved->kind == TYPE_VEC)
            return 1;
        return 0; // Primitives, etc.
    }

    if (strcmp(name, "string") == 0)
        return 1;
    if (type_is_builtin_name(name))
        return 0;
    if (is_enum_type_name(gen, name))
        return enum_has_rc_fields(gen, name);
    return 1;
}

// Resolve a type node to its enum name under substitution, or NULL if not an enum.
// For NODE_GENERIC_TYPE: builds mangled name and checks. For NODE_IDENT with subst_ctx: resolves.
// Returns a static/interned name (caller must NOT free).
const char* resolve_enum_name(CodeGen* gen, Node* type_node) {
    if (!type_node)
        return NULL;
    type_node = resolve_alias(gen, type_node);
    if (type_node->type == NODE_GENERIC_TYPE) {
        char* mangled = build_mangled_name_from_generic_node(gen, type_node);
        if (is_enum_type_name(gen, mangled)) {
            // Return the registered name from enum_names (don't free mangled yet)
            for (int i = 0; i < gen->enums.count; i++) {
                if (strcmp(gen->enums.names[i], mangled) == 0) {
                    free(mangled);
                    return gen->enums.names[i];
                }
            }
        }
        free(mangled);
        return NULL;
    }
    if (type_node->type == NODE_IDENT) {
        const char* name     = type_node->as.ident.name;
        Type*       resolved = subst_lookup(gen, name);
        if (resolved) {
            if (resolved->kind == TYPE_ENUM)
                return resolved->as.enm.name;
            return NULL;
        }
        if (is_enum_type_name(gen, name))
            return name;
    }
    return NULL;
}

// Helper to check if a name is a type variable (not a builtin type)
int codegen_is_type_variable(const char* name) {
    return !type_is_builtin_name(name);
}
