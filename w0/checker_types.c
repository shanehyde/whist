#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "checker_internal.h"
#include "vec.h"

// =============================================================================
// Generic struct helpers
// =============================================================================

// Register a generic struct definition
void register_generic_def(Checker* checker, const char* name, char** type_params,
                          char** type_param_bounds, int type_param_count, Node* decl) {
    VEC_GROW(checker->generic_defs, checker->generic_def_count, checker->generic_def_capacity);
    GenericDef* def        = &checker->generic_defs[checker->generic_def_count++];
    def->name              = xstrdup(name);
    def->type_params       = xmalloc(type_param_count * sizeof(char*));
    def->type_param_bounds = xmalloc(type_param_count * sizeof(char*));
    for (int i = 0; i < type_param_count; i++) {
        def->type_params[i] = xstrdup(type_params[i]);
        def->type_param_bounds[i] =
            (type_param_bounds && type_param_bounds[i]) ? xstrdup(type_param_bounds[i]) : NULL;
    }
    def->type_param_count = type_param_count;
    def->decl             = decl;
    def->is_type_alias    = 0;
    def->methods          = NULL;
    def->method_count     = 0;
    def->method_capacity  = 0;
}

// Register a method on a generic struct definition
void register_generic_method(GenericDef* def, Node* method) {
    VEC_GROW(def->methods, def->method_count, def->method_capacity);
    def->methods[def->method_count++] = method;
}

// Look up a generic struct definition by name
GenericDef* lookup_generic_def(Checker* checker, const char* name) {
    for (int i = 0; i < checker->generic_def_count; i++) {
        if (strcmp(checker->generic_defs[i].name, name) == 0) {
            return &checker->generic_defs[i];
        }
    }
    return NULL;
}

// Look up an already-instantiated generic struct by mangled name
GenericInstance* lookup_generic_instance(Checker* checker, const char* mangled_name) {
    for (int i = 0; i < checker->generic_instance_count; i++) {
        if (strcmp(checker->generic_instances[i].mangled_name, mangled_name) == 0) {
            return &checker->generic_instances[i];
        }
    }
    return NULL;
}

// Look up an already-instantiated span type by mangled name
static SpanInstance* lookup_span_instance(Checker* checker, const char* mangled_name) {
    for (int i = 0; i < checker->span_instance_count; i++) {
        if (strcmp(checker->span_instances[i].mangled_name, mangled_name) == 0) {
            return &checker->span_instances[i];
        }
    }
    return NULL;
}

// Register an instantiated span type
static void register_span_instance(Checker* checker, const char* mangled_name, Type* elem_type,
                                   Type* span_type) {
    VEC_GROW(checker->span_instances, checker->span_instance_count,
             checker->span_instance_capacity);
    SpanInstance* inst = &checker->span_instances[checker->span_instance_count++];
    inst->mangled_name = xstrdup(mangled_name);
    inst->elem_type    = elem_type;
    inst->type         = span_type;
}

// Look up an already-instantiated vec type by mangled name
static VecInstance* lookup_vec_instance(Checker* checker, const char* mangled_name) {
    for (int i = 0; i < checker->vec_instance_count; i++) {
        if (strcmp(checker->vec_instances[i].mangled_name, mangled_name) == 0) {
            return &checker->vec_instances[i];
        }
    }
    return NULL;
}

// Register an instantiated vec type
static void register_vec_instance(Checker* checker, const char* mangled_name, Type* elem_type,
                                  Type* vec_type) {
    VEC_GROW(checker->vec_instances, checker->vec_instance_count, checker->vec_instance_capacity);
    VecInstance* inst  = &checker->vec_instances[checker->vec_instance_count++];
    inst->mangled_name = xstrdup(mangled_name);
    inst->elem_type    = elem_type;
    inst->type         = vec_type;
}

// Register an instantiated generic struct
static void register_generic_instance(Checker* checker, const char* mangled_name,
                                      const char* base_name, Type* type, Type** type_args,
                                      int type_arg_count) {
    VEC_GROW(checker->generic_instances, checker->generic_instance_count,
             checker->generic_instance_capacity);
    GenericInstance* inst = &checker->generic_instances[checker->generic_instance_count++];
    inst->mangled_name    = xstrdup(mangled_name);
    inst->base_name       = xstrdup(base_name);
    inst->type            = type;
    inst->type_args       = xmalloc(type_arg_count * sizeof(Type*));
    for (int i = 0; i < type_arg_count; i++) {
        inst->type_args[i] = type_args[i];
    }
    inst->type_arg_count = type_arg_count;
}

// =============================================================================
// Type resolution
// =============================================================================

// Helper to check if an identifier is a type variable (not a builtin or defined type)
static int is_type_variable(Checker* checker, const char* name) {
    if (type_builtin_from_name(name))
        return 0;
    Symbol* sym = checker_lookup_any(checker, name);
    if (sym && sym->kind == SYM_TYPE)
        return 0;
    return 1;
}

// Try to match a method's receiver type args against concrete instantiation args.
// pattern_args: the method's receiver_type_args (e.g., <i32, Box<T>>)
// concrete_args: the resolved types for instantiation (e.g., [i32, Box_i64])
// def: the generic struct definition (for type param names)
// On success, sets out_params/out_args/out_count to the extracted bindings
// Returns 1 on success, 0 on failure (pattern doesn't match)
static int unify_method_pattern(Checker* checker, NodeList* pattern_args, Type** concrete_args,
                                int arg_count, char*** out_params, Type*** out_args,
                                int* out_count) {
    // Collect type variable bindings
    int    binding_capacity = 4;
    int    binding_count    = 0;
    char** bound_names      = xmalloc(binding_capacity * sizeof(char*));
    Type** bound_types      = xmalloc(binding_capacity * sizeof(Type*));

    // Process each argument position
    for (int i = 0; i < arg_count; i++) {
        Node* pattern  = pattern_args->nodes[i];
        Type* concrete = concrete_args[i];

        // Case 1: Pattern is a simple identifier
        if (pattern->type == NODE_IDENT) {
            const char* name = pattern->as.ident.name;

            // Check if it's a concrete type (builtin or defined)
            Type* builtin = type_builtin_from_name(name);
            if (builtin) {
                // Must match exactly
                if (!type_equals(builtin, concrete)) {
                    free(bound_names);
                    free(bound_types);
                    return 0;
                }
                continue;
            }

            // Check if it's a defined type
            Symbol* sym = checker_lookup_any(checker, name);
            if (sym && sym->kind == SYM_TYPE) {
                // Must match exactly
                if (!type_equals(sym->type, concrete)) {
                    free(bound_names);
                    free(bound_types);
                    return 0;
                }
                continue;
            }

            // It's a type variable - bind it
            // Check if already bound
            int found = 0;
            for (int b = 0; b < binding_count; b++) {
                if (strcmp(bound_names[b], name) == 0) {
                    // Already bound - must match
                    if (!type_equals(bound_types[b], concrete)) {
                        free(bound_names);
                        free(bound_types);
                        return 0;
                    }
                    found = 1;
                    break;
                }
            }
            if (!found) {
                // New binding
                if (binding_count >= binding_capacity) {
                    binding_capacity *= 2;
                    bound_names = xrealloc(bound_names, binding_capacity * sizeof(char*));
                    bound_types = xrealloc(bound_types, binding_capacity * sizeof(Type*));
                }
                bound_names[binding_count] = xstrdup(name);
                bound_types[binding_count] = concrete;
                binding_count++;
            }
        }
        // Case 2: Pattern is a generic type (e.g., Box<T>)
        else if (pattern->type == NODE_GENERIC_TYPE) {
            // Concrete must be a struct with matching base name
            if (concrete->kind != TYPE_STRUCT) {
                free(bound_names);
                free(bound_types);
                return 0;
            }

            // Build expected mangled name prefix
            const char* pattern_base = pattern->as.generic_type.base_name;

            // Check if concrete struct name starts with pattern base
            if (strncmp(concrete->as.struc.name, pattern_base, strlen(pattern_base)) != 0 ||
                concrete->as.struc.name[strlen(pattern_base)] != '_') {
                free(bound_names);
                free(bound_types);
                return 0;
            }

            // For now, handle simple case: Box<T> matching Box_i64
            // Extract the type argument from the mangled name
            int nested_arg_count = pattern->as.generic_type.type_args.count;
            if (nested_arg_count == 1) {
                Node* nested_pattern = pattern->as.generic_type.type_args.nodes[0];
                if (nested_pattern->type == NODE_IDENT &&
                    is_type_variable(checker, nested_pattern->as.ident.name)) {
                    // Extract the type from mangled name
                    const char* type_suffix = concrete->as.struc.name + strlen(pattern_base) + 1;
                    Type*       extracted   = type_builtin_from_name(type_suffix);
                    if (!extracted) {
                        // Could be a user-defined type or another struct
                        Symbol* sym = checker_lookup_any(checker, type_suffix);
                        if (sym && sym->kind == SYM_TYPE) {
                            extracted = sym->type;
                        } else {
                            // Try as a mangled struct name
                            extracted = type_struct(type_suffix);
                        }
                    }

                    // Bind the type variable
                    const char* var_name = nested_pattern->as.ident.name;
                    int         found    = 0;
                    for (int b = 0; b < binding_count; b++) {
                        if (strcmp(bound_names[b], var_name) == 0) {
                            if (!type_equals(bound_types[b], extracted)) {
                                free(bound_names);
                                free(bound_types);
                                return 0;
                            }
                            found = 1;
                            break;
                        }
                    }
                    if (!found) {
                        if (binding_count >= binding_capacity) {
                            binding_capacity *= 2;
                            bound_names = xrealloc(bound_names, binding_capacity * sizeof(char*));
                            bound_types = xrealloc(bound_types, binding_capacity * sizeof(Type*));
                        }
                        bound_names[binding_count] = xstrdup(var_name);
                        bound_types[binding_count] = extracted;
                        binding_count++;
                    }
                }
            }
        }
    }

    *out_params = bound_names;
    *out_args   = bound_types;
    *out_count  = binding_count;
    return 1;
}

// Instantiate a generic enum with resolved type arguments.
// def: the generic definition (must be NODE_ENUM_DECL)
// mangled: the mangled name (takes ownership)
// resolved_args: array of resolved type arguments (takes ownership)
// arg_count: number of type arguments
// Returns the instantiated enum type or type_error.
Type* instantiate_generic_enum(Checker* checker, GenericDef* def, char* mangled,
                               Type** resolved_args, int arg_count) {
    Type* enum_type = type_enum(mangled);

    // Register early to handle recursive types
    register_generic_instance(checker, mangled, def->name, enum_type, resolved_args, arg_count);

    // Set up substitution context
    char** old_params = checker->current_type_params;
    Type** old_args   = checker->current_type_args;
    int    old_count  = checker->current_type_param_count;

    checker->current_type_params      = def->type_params;
    checker->current_type_args        = resolved_args;
    checker->current_type_param_count = def->type_param_count;

    // Process enum variants
    Node* decl        = def->decl;
    int   value_count = decl->as.enum_decl.values.count;

    enum_type->as.enm.value_count         = value_count;
    enum_type->as.enm.value_names         = xmalloc(value_count * sizeof(char*));
    enum_type->as.enm.variant_types       = xmalloc(value_count * sizeof(Type**));
    enum_type->as.enm.variant_type_counts = xmalloc(value_count * sizeof(int));

    for (int i = 0; i < value_count; i++) {
        Node* val                        = decl->as.enum_decl.values.nodes[i];
        enum_type->as.enm.value_names[i] = xstrdup(val->as.enum_variant.name);

        int type_count                           = val->as.enum_variant.types.count;
        enum_type->as.enm.variant_type_counts[i] = type_count;

        if (type_count > 0) {
            enum_type->as.enm.has_data         = 1;
            enum_type->as.enm.variant_types[i] = xmalloc(type_count * sizeof(Type*));
            for (int j = 0; j < type_count; j++) {
                Type* resolved = resolve_type(checker, val->as.enum_variant.types.nodes[j]);
                enum_type->as.enm.variant_types[i][j] = resolved;
                if (resolved && (resolved->kind == TYPE_STRUCT ||
                                 (resolved->kind == TYPE_ENUM && resolved->as.enm.has_rc_fields))) {
                    enum_type->as.enm.has_rc_fields = 1;
                }
            }
        } else {
            enum_type->as.enm.variant_types[i] = NULL;
        }
    }

    // Restore substitution context
    checker->current_type_params      = old_params;
    checker->current_type_args        = old_args;
    checker->current_type_param_count = old_count;

    // Register in symbol table
    checker_define(checker, mangled, SYM_TYPE, enum_type, 0, 1, checker->current_module);

    free(mangled);
    free(resolved_args);
    return enum_type;
}

static Type* resolve_vec_type(Checker* checker, Node* type_node) {
    int arg_count = type_node->as.generic_type.type_args.count;
    if (arg_count != 1) {
        check_error(checker, type_node->line, type_node->column,
                    "Vec requires exactly 1 type argument, got %d", arg_count);
        return type_error;
    }

    // Resolve element type
    Type* elem_type = resolve_type(checker, type_node->as.generic_type.type_args.nodes[0]);
    if (elem_type == type_error) {
        return type_error;
    }

    // Generate mangled name for the vec instance
    char mangled[256];
    snprintf(mangled, sizeof(mangled), "Vec_%s", type_name(elem_type));

    // Check if already instantiated
    VecInstance* existing = lookup_vec_instance(checker, mangled);
    if (existing) {
        return existing->type;
    }

    // Create the vec type
    Type* vec_type = type_vec(elem_type);

    // Register for codegen
    register_vec_instance(checker, mangled, elem_type, vec_type);

    // Also ensure a Span instance for the same element type exists (for slicing)
    char span_mangled[256];
    snprintf(span_mangled, sizeof(span_mangled), "Span_%s", type_name(elem_type));
    if (!lookup_span_instance(checker, span_mangled)) {
        Type* span_type = type_span(elem_type);
        register_span_instance(checker, span_mangled, elem_type, span_type);
    }

    return vec_type;
}

static Type* resolve_span_type(Checker* checker, Node* type_node) {
    int arg_count = type_node->as.generic_type.type_args.count;
    if (arg_count != 1) {
        check_error(checker, type_node->line, type_node->column,
                    "Span requires exactly 1 type argument, got %d", arg_count);
        return type_error;
    }

    // Resolve element type
    Type* elem_type = resolve_type(checker, type_node->as.generic_type.type_args.nodes[0]);
    if (elem_type == type_error) {
        return type_error;
    }

    // Generate mangled name for the span instance
    char mangled[256];
    snprintf(mangled, sizeof(mangled), "Span_%s", type_name(elem_type));

    // Check if already instantiated
    SpanInstance* existing = lookup_span_instance(checker, mangled);
    if (existing) {
        return existing->type;
    }

    // Create the span type
    Type* span_type = type_span(elem_type);

    // Register for codegen
    register_span_instance(checker, mangled, elem_type, span_type);

    return span_type;
}

static Type* resolve_generic_type_alias(Checker* checker, Node* type_node, GenericDef* def,
                                        Type** resolved_args) {
    const char* base_name = type_node->as.generic_type.base_name;

    checker->alias_depth++;
    if (checker->alias_depth > 16) {
        check_error(checker, type_node->line, type_node->column, "Recursive type alias '%s'",
                    base_name);
        checker->alias_depth--;
        free(resolved_args);
        return type_error;
    }

    // Set up substitution context
    char** old_params = checker->current_type_params;
    Type** old_args   = checker->current_type_args;
    int    old_count  = checker->current_type_param_count;

    checker->current_type_params      = def->type_params;
    checker->current_type_args        = resolved_args;
    checker->current_type_param_count = def->type_param_count;

    Type* result = resolve_type(checker, def->decl->as.type_alias.target_type);

    // Restore context
    checker->current_type_params      = old_params;
    checker->current_type_args        = old_args;
    checker->current_type_param_count = old_count;

    checker->alias_depth--;
    free(resolved_args);
    return result;
}

static Type* instantiate_generic_struct(Checker* checker, Node* type_node, GenericDef* def,
                                        char* mangled, Type** resolved_args, int arg_count) {
    const char* base_name = type_node->as.generic_type.base_name;

    // Instantiate: create a new TYPE_STRUCT with substituted field types
    Type* struct_type = type_struct(mangled);

    // Register early to handle recursive types
    register_generic_instance(checker, mangled, base_name, struct_type, resolved_args, arg_count);

    // Set up substitution context
    char** old_params = checker->current_type_params;
    Type** old_args   = checker->current_type_args;
    int    old_count  = checker->current_type_param_count;

    checker->current_type_params      = def->type_params;
    checker->current_type_args        = resolved_args;
    checker->current_type_param_count = def->type_param_count;

    // Resolve field types with substitution
    Node* decl        = def->decl;
    int   field_count = decl->as.struct_decl.fields.count;

    struct_type->as.struc.field_count = field_count;
    struct_type->as.struc.field_names = xmalloc(field_count * sizeof(char*));
    struct_type->as.struc.field_types = xmalloc(field_count * sizeof(Type*));

    for (int i = 0; i < field_count; i++) {
        Node* field                          = decl->as.struct_decl.fields.nodes[i];
        struct_type->as.struc.field_names[i] = xstrdup(field->as.field.name);
        struct_type->as.struc.field_types[i] = resolve_type(checker, field->as.field.type);
    }

    // Check if any field is an RC-managed type (struct, Vec, or enum with RC fields)
    for (int i = 0; i < field_count; i++) {
        Type* ftype = struct_type->as.struc.field_types[i];
        if (ftype && (ftype->kind == TYPE_STRUCT || ftype->kind == TYPE_VEC ||
                      (ftype->kind == TYPE_ENUM && ftype->as.enm.has_rc_fields))) {
            struct_type->as.struc.has_rc_fields = 1;
            break;
        }
    }

    // Check if the base generic has a Drop trait impl and propagate to instantiation
    for (int ti = 0; ti < checker->trait_impl_count; ti++) {
        if (strcmp(checker->trait_impls[ti].trait_name, "Drop") == 0 &&
            strcmp(checker->trait_impls[ti].type_name, base_name) == 0) {
            struct_type->as.struc.has_drop = 1;
            break;
        }
    }

    // Instantiate methods on the generic struct
    for (int m = 0; m < def->method_count; m++) {
        Node*           method_decl = def->methods[m];
        func_decl_node* mfdn        = &method_decl->as.func_decl;

        // Try to match method's receiver pattern against concrete args
        char** method_params = NULL;
        Type** method_args   = NULL;
        int    method_count  = 0;

        if (!unify_method_pattern(checker, &mfdn->receiver_type_args, resolved_args, arg_count,
                                  &method_params, &method_args, &method_count)) {
            // Pattern doesn't match - skip this method for this instantiation
            continue;
        }

        // Combine struct params with method-specific bindings for substitution
        int    combined_count  = def->type_param_count + method_count;
        char** combined_params = xmalloc(combined_count * sizeof(char*));
        Type** combined_args   = xmalloc(combined_count * sizeof(Type*));

        // First, add the struct's type params
        for (int i = 0; i < def->type_param_count; i++) {
            combined_params[i] = def->type_params[i];
            combined_args[i]   = resolved_args[i];
        }
        // Then add method-specific bindings
        for (int i = 0; i < method_count; i++) {
            combined_params[def->type_param_count + i] = method_params[i];
            combined_args[def->type_param_count + i]   = method_args[i];
        }

        // Set up combined substitution context
        checker->current_type_params      = combined_params;
        checker->current_type_args        = combined_args;
        checker->current_type_param_count = combined_count;

        // Build function type with substituted types
        int    param_count = mfdn->params.count;
        Type** param_types = NULL;
        if (param_count > 0) {
            param_types = xmalloc(param_count * sizeof(Type*));
            for (int p = 0; p < param_count; p++) {
                Node* param = mfdn->params.nodes[p];
                param_types[p] =
                    param->as.param.type ? resolve_type(checker, param->as.param.type) : type_void;
            }
        }

        Type* return_type =
            mfdn->return_type ? resolve_type(checker, mfdn->return_type) : type_void;

        // Type-check the method body with the combined substitution context
        if (mfdn->body) {
            checker_push_scope(checker);
            Type* old_return             = checker->current_func_return;
            checker->current_func_return = return_type;

            char** old_accessible_modules             = checker->current_accessible_modules;
            int    old_accessible_modules_count       = checker->current_accessible_modules_count;
            checker->current_accessible_modules       = mfdn->accessible_modules;
            checker->current_accessible_modules_count = mfdn->accessible_modules_count;

            // Inject 'self' into scope
            checker_define(checker, "self", SYM_VAR, struct_type, mfdn->receiver_is_const, 0, NULL);

            // Define parameters
            for (int p = 0; p < param_count; p++) {
                Node* param = mfdn->params.nodes[p];
                checker_define(checker, param->as.param.name, SYM_VAR, param_types[p],
                               param->as.param.is_const, 0, NULL);
            }

            // Check body statements
            for (int s = 0; s < mfdn->body->as.block.stmts.count; s++) {
                check_statement(checker, mfdn->body->as.block.stmts.nodes[s]);
            }

            checker->current_accessible_modules       = old_accessible_modules;
            checker->current_accessible_modules_count = old_accessible_modules_count;
            checker->current_func_return              = old_return;
            checker_pop_scope(checker);
        }

        // Restore struct-only context
        checker->current_type_params      = def->type_params;
        checker->current_type_args        = resolved_args;
        checker->current_type_param_count = def->type_param_count;

        // Free combined arrays (but not the strings - they're borrowed or will be freed later)
        free(combined_params);
        free(combined_args);
        for (int i = 0; i < method_count; i++) {
            free(method_params[i]);
        }
        free(method_params);
        free(method_args);

        Type* method_type = type_func(param_types, param_count, return_type, 0);

        // Register the method on the struct type
        int n = struct_type->as.struc.method_count;
        struct_type->as.struc.method_names =
            xrealloc(struct_type->as.struc.method_names, (n + 1) * sizeof(char*));
        struct_type->as.struc.method_types =
            xrealloc(struct_type->as.struc.method_types, (n + 1) * sizeof(Type*));
        struct_type->as.struc.method_is_const =
            xrealloc(struct_type->as.struc.method_is_const, (n + 1) * sizeof(int));

        struct_type->as.struc.method_names[n]    = xstrdup(mfdn->name);
        struct_type->as.struc.method_types[n]    = method_type;
        struct_type->as.struc.method_is_const[n] = mfdn->receiver_is_const;
        struct_type->as.struc.method_count       = n + 1;

        // Also register the mangled function name in the symbol table
        char*  method_mangled  = type_mangle_generic(mfdn->receiver_type, resolved_args, arg_count);
        size_t method_name_len = strlen(method_mangled) + 1 + strlen(mfdn->name) + 1;
        char*  full_method_name = xmalloc(method_name_len);
        snprintf(full_method_name, method_name_len, "%s_%s", method_mangled, mfdn->name);

        checker_define(checker, full_method_name, SYM_FUNC, method_type, 1, mfdn->is_public,
                       checker->current_module);

        free(method_mangled);
        free(full_method_name);
    }

    // Restore substitution context
    checker->current_type_params      = old_params;
    checker->current_type_args        = old_args;
    checker->current_type_param_count = old_count;

    // Register in symbol table so it can be looked up
    checker_define(checker, mangled, SYM_TYPE, struct_type, 0, 1, checker->current_module);

    free(mangled);
    free(resolved_args);
    return struct_type;
}

Type* resolve_type(Checker* checker, Node* type_node) {
    if (!type_node)
        return type_void;

    switch (type_node->type) {
    case NODE_IDENT: {
        const char* name = type_node->as.ident.name;

        // Check for builtin type
        Type* builtin = type_builtin_from_name(name);
        if (builtin)
            return builtin;

        // Check for type parameter substitution (when instantiating generics)
        if (checker->current_type_params) {
            for (int i = 0; i < checker->current_type_param_count; i++) {
                if (strcmp(checker->current_type_params[i], name) == 0) {
                    return checker->current_type_args[i];
                }
            }
        }

        // Look up user-defined type
        Symbol* sym = checker_lookup(checker, name);
        if (sym && sym->kind == SYM_TYPE) {
            return sym->type;
        }
        check_error(checker, type_node->line, type_node->column, "Unknown type '%s'", name);
        return type_error;
    }
    case NODE_GENERIC_TYPE: {
        // Generic type instantiation: Box<i64>, Pair<K, V>, Span<T>
        const char* base_name = type_node->as.generic_type.base_name;
        int         arg_count = type_node->as.generic_type.type_args.count;

        // Handle builtin Vec<T> type
        if (strcmp(base_name, "Vec") == 0) {
            return resolve_vec_type(checker, type_node);
        }

        // Handle builtin Span<T> type
        if (strcmp(base_name, "Span") == 0) {
            return resolve_span_type(checker, type_node);
        }

        // Look up the generic definition
        GenericDef* def = lookup_generic_def(checker, base_name);
        if (!def) {
            // Maybe it's not a generic - error
            check_error(checker, type_node->line, type_node->column, "Unknown generic type '%s'",
                        base_name);
            return type_error;
        }

        // Check arity
        if (arg_count != def->type_param_count) {
            check_error(checker, type_node->line, type_node->column,
                        "Generic type '%s' expects %d type arguments, got %d", base_name,
                        def->type_param_count, arg_count);
            return type_error;
        }

        // Resolve all type arguments
        Type** resolved_args = xmalloc(arg_count * sizeof(Type*));
        for (int i = 0; i < arg_count; i++) {
            resolved_args[i] = resolve_type(checker, type_node->as.generic_type.type_args.nodes[i]);
            if (resolved_args[i] == type_error) {
                free(resolved_args);
                return type_error;
            }
        }

        // Check trait bounds on type arguments
        for (int i = 0; i < arg_count; i++) {
            if (def->type_param_bounds[i]) {
                const char* bound             = def->type_param_bounds[i];
                const char* arg_type_name_str = (resolved_args[i]->kind == TYPE_STRUCT)
                                                    ? resolved_args[i]->as.struc.name
                                                    : NULL;
                int         satisfied         = 0;
                if (arg_type_name_str) {
                    for (int t = 0; t < checker->trait_impl_count; t++) {
                        if (strcmp(checker->trait_impls[t].trait_name, bound) == 0 &&
                            strcmp(checker->trait_impls[t].type_name, arg_type_name_str) == 0) {
                            satisfied = 1;
                            break;
                        }
                    }
                }
                if (!satisfied) {
                    check_error(checker, type_node->line, type_node->column,
                                "Type '%s' does not implement trait '%s'",
                                type_name(resolved_args[i]), bound);
                    free(resolved_args);
                    return type_error;
                }
            }
        }

        // Handle generic type alias: substitute type params and resolve target
        if (def->is_type_alias) {
            return resolve_generic_type_alias(checker, type_node, def, resolved_args);
        }

        // Generate mangled name
        char* mangled = type_mangle_generic(base_name, resolved_args, arg_count);

        // Check if already instantiated
        GenericInstance* existing = lookup_generic_instance(checker, mangled);
        if (existing) {
            free(mangled);
            free(resolved_args);
            return existing->type;
        }

        // Branch on enum vs struct
        if (def->decl->type == NODE_ENUM_DECL) {
            return instantiate_generic_enum(checker, def, mangled, resolved_args, arg_count);
        }

        return instantiate_generic_struct(checker, type_node, def, mangled, resolved_args,
                                          arg_count);
    }
    case NODE_UNARY:
        // Pointer types no longer supported
        check_error(checker, type_node->line, type_node->column,
                    "Pointer types are no longer supported");
        return type_error;
    case NODE_ARRAY_TYPE: {
        Type* elem = resolve_type(checker, type_node->as.array_type.elem_type);
        int   size = -1;
        if (type_node->as.array_type.size) {
            // For now, only support constant integer sizes
            if (type_node->as.array_type.size->type == NODE_INT_LIT) {
                size = (int)type_node->as.array_type.size->as.int_lit.value;
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
