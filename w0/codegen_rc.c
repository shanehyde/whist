#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "codegen_internal.h"
#include "types.h"
#include "vec.h"

// =============================================================================
// RC (Reference Counting) Variable Tracking
// =============================================================================
//
// The codegen tracks RC-managed variables (those created via `new`) to emit
// automatic cleanup calls at scope exits and function returns. This acts as
// a compile-time deterministic "GC" — each RC variable is paired with a
// decrement function (either generic `__rc_dec` or type-specific
// `__rc_dec_TypeName` for types with Drop or nested RC fields).
//
// Key functions:
//   rc_push_var     - Register a new RC variable with its scope depth
//   rc_cleanup_scope - Emit dec calls for vars at a given depth, remove them
//   rc_cleanup_all   - Emit dec calls for ALL vars (before return), keep list
//   rc_clear_all     - Free the tracking list (at function boundaries)
//
// rc_cleanup_scope modifies the var list (compacts it); rc_cleanup_all only
// emits code without modifying the list. This distinction matters because
// rc_cleanup_all is used before early returns in conditional branches where
// the var list must remain intact for the non-returning path.

// Return the appropriate __rc_dec function name for a type (type-specific or generic)
const char* get_dec_func_for_type(Type* t) {
    if (t && t->kind == TYPE_STRUCT && (t->as.struc.has_drop || t->as.struc.has_rc_fields)) {
        // Build "__rc_dec_TypeName"
        size_t len = strlen("__rc_dec_") + strlen(t->as.struc.name) + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__rc_dec_%s", t->as.struc.name);
        return buf;
    }
    if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
        size_t len = strlen("__rc_dec_") + strlen(t->as.enm.name) + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__rc_dec_%s", t->as.enm.name);
        return buf;
    }
    if (t && t->kind == TYPE_STRINGBUILDER) {
        return xstrdup("__rc_dec_StringBuilder");
    }
    if (t && t->kind == TYPE_VEC) {
        const char* elem_tname = type_mangle_name(t->as.vec.elem);
        size_t      len        = strlen("__rc_dec_Vec_") + strlen(elem_tname) + 1;
        char*       buf        = xmalloc(len);
        snprintf(buf, len, "__rc_dec_Vec_%s", elem_tname);
        return buf;
    }
    return xstrdup("__rc_dec");
}

// Return the appropriate __rc_inc function name for a type (enum-specific or generic)
const char* get_inc_func_for_type(Type* t) {
    if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
        size_t len = strlen("__rc_inc_") + strlen(t->as.enm.name) + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__rc_inc_%s", t->as.enm.name);
        return buf;
    }
    return xstrdup("__rc_inc");
}

// Register an RC-managed variable for scope-based cleanup tracking
void rc_push_var(CodeGen* gen, const char* name, const char* dec_func, Type* type) {
    VEC_GROW(gen->rc.vars, gen->rc.count, gen->rc.capacity);
    gen->rc.vars[gen->rc.count].name        = xstrdup(name);
    gen->rc.vars[gen->rc.count].dec_func    = xstrdup(dec_func);
    gen->rc.vars[gen->rc.count].type        = type;
    gen->rc.vars[gen->rc.count].scope_depth = gen->rc.depth;
    gen->rc.count++;
}

// Emit dec for vars at the given depth, remove them from list
void rc_cleanup_scope(CodeGen* gen, int depth) {
    int dst = 0;
    for (int i = 0; i < gen->rc.count; i++) {
        if (gen->rc.vars[i].scope_depth == depth) {
            emit_indent(gen);
            emit(gen, "%s(%s);\n", gen->rc.vars[i].dec_func, gen->rc.vars[i].name);
            free(gen->rc.vars[i].name);
            free(gen->rc.vars[i].dec_func);
        } else {
            gen->rc.vars[dst++] = gen->rc.vars[i];
        }
    }
    gen->rc.count = dst;
}

// Emit dec for ALL remaining vars (skip one by name). Does NOT modify list.
void rc_cleanup_all(CodeGen* gen, const char* skip_name) {
    for (int i = 0; i < gen->rc.count; i++) {
        if (skip_name && strcmp(gen->rc.vars[i].name, skip_name) == 0) {
            continue;
        }
        emit_indent(gen);
        emit(gen, "%s(%s);\n", gen->rc.vars[i].dec_func, gen->rc.vars[i].name);
    }
}

// Free and reset the RC var list (at function boundary)
void rc_clear_all(CodeGen* gen) {
    for (int i = 0; i < gen->rc.count; i++) {
        free(gen->rc.vars[i].name);
        free(gen->rc.vars[i].dec_func);
    }
    gen->rc.count = 0;
    gen->rc.depth = 0;
}

// Look up the stored dec_func for a tracked RC variable
const char* rc_get_dec_func(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->rc.count; i++) {
        if (strcmp(gen->rc.vars[i].name, name) == 0)
            return gen->rc.vars[i].dec_func;
    }
    return "__rc_dec";
}

// Look up the stored Type* for a tracked RC variable
Type* rc_get_var_type(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->rc.count; i++) {
        if (strcmp(gen->rc.vars[i].name, name) == 0)
            return gen->rc.vars[i].type;
    }
    return NULL;
}

// Check if a variable name is in the RC tracking list
int rc_is_tracked(CodeGen* gen, const char* name) {
    for (int i = 0; i < gen->rc.count; i++) {
        if (strcmp(gen->rc.vars[i].name, name) == 0)
            return 1;
    }
    return 0;
}

// =============================================================================
// RC Emission — Runtime, Enum/Struct/Vec RC helpers
// =============================================================================

// RC runtime (__RcHeader, __rc_alloc, __rc_inc, __rc_dec) is now provided
// by whist_runtime.h — this function is intentionally empty.
void emit_rc_runtime(CodeGen* gen) {
    (void)gen;
}

// Emit one __rc_inc_E or __rc_dec_E function body for an enum with RC-managed variant fields.
// `is_inc` selects between inc/dec; `ename` is the (possibly mangled) enum name;
// `enum_decl` supplies the variant list. The caller must set gen->generics.subst for generics.
static void emit_enum_rc_func(CodeGen* gen, const char* ename, Node* enum_decl, int is_inc) {
    const char* op = is_inc ? "inc" : "dec";
    emit(gen, "static inline void __rc_%s_%s(%s v) {\n", op, ename, ename);
    emit(gen, "    switch (v.tag) {\n");
    for (int vi = 0; vi < enum_decl->as.enum_decl.values.count; vi++) {
        Node* var = enum_decl->as.enum_decl.values.nodes[vi];
        if (var->as.enum_variant.types.count == 0)
            continue;
        emit(gen, "    case %s_%.*s:\n", ename, var->as.enum_variant.name_length,
             var->as.enum_variant.name);
        for (int t = 0; t < var->as.enum_variant.types.count; t++) {
            Node* tnode = var->as.enum_variant.types.nodes[t];
            if (!type_node_has_rc(gen, tnode))
                continue;
            // Determine if the field's type is itself an enum requiring type-specific RC
            const char* enum_nm = resolve_enum_name(gen, tnode);
            if (!enum_nm && tnode->type == NODE_IDENT &&
                is_enum_type_name(gen, tnode->as.ident.name)) {
                enum_nm = tnode->as.ident.name;
            }
            if (enum_nm) {
                emit(gen, "        __rc_%s_%s(v.%.*s.f%d);\n", op, enum_nm,
                     var->as.enum_variant.name_length, var->as.enum_variant.name, t);
            } else {
                emit(gen, "        __rc_%s(v.%.*s.f%d);\n", op, var->as.enum_variant.name_length,
                     var->as.enum_variant.name, t);
            }
        }
        emit(gen, "        break;\n");
    }
    emit(gen, "    default: break;\n");
    emit(gen, "    }\n");
    emit(gen, "}\n\n");
}

// Emit __rc_inc_EnumName/__rc_dec_EnumName helpers for enums with RC-managed fields
void emit_enum_rc_helpers(CodeGen* gen, Node* ast) {
    // Forward declarations for non-generic enums
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue; // Skip generic templates
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_rc_fields(gen, ename))
                continue;
            emit(gen, "static inline void __rc_inc_%s(%s v);\n", ename, ename);
            emit(gen, "static inline void __rc_dec_%s(%s v);\n", ename, ename);
        }
    }
    // Forward declarations for generic enum RC helpers
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_rc_fields(gen, info->mangled_name))
            continue;
        emit(gen, "static inline void __rc_inc_%s(%s v);\n", info->mangled_name,
             info->mangled_name);
        emit(gen, "static inline void __rc_dec_%s(%s v);\n", info->mangled_name,
             info->mangled_name);
    }
    if (gen->enums.count > 0)
        emit(gen, "\n");

    // Definitions for non-generic enums
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_ENUM_DECL)
                continue;
            if (decl->as.enum_decl.type_param_count > 0)
                continue;
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_rc_fields(gen, ename))
                continue;
            emit_enum_rc_func(gen, ename, decl, 1); // __rc_inc
            emit_enum_rc_func(gen, ename, decl, 0); // __rc_dec
        }
    }

    // Definitions for generic enum instances
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_rc_fields(gen, info->mangled_name))
            continue;
        Node* tmpl = find_generic_enum_decl(ast, info->base_name);
        if (!tmpl)
            continue;

        // Set up substitution context for generic type resolution
        TypeSubstContext subst;
        subst.type_params = tmpl->as.enum_decl.type_params;
        subst.type_args   = info->type_args;
        subst.count       = tmpl->as.enum_decl.type_param_count;

        TypeSubstContext* old_subst = gen->generics.subst;
        gen->generics.subst         = &subst;

        emit_enum_rc_func(gen, info->mangled_name, tmpl, 1); // __rc_inc
        emit_enum_rc_func(gen, info->mangled_name, tmpl, 0); // __rc_dec

        gen->generics.subst = old_subst;
    }
}

// Check if a struct type has a Drop trait implementation
static int struct_implements_drop(CodeGen* gen, const char* type_name) {
    for (int t = 0; t < gen->checker.trait_count; t++) {
        if (strcmp(gen->checker.traits[t].trait_name, "Drop") == 0 &&
            strcmp(gen->checker.traits[t].type_name, type_name) == 0)
            return 1;
    }
    return 0;
}

typedef struct {
    char* field_name;
    char* type_name;
    int   is_enum;
} RcFieldInfo;

// Collect info about RC-managed fields in a struct for __rc_dec generation
static int collect_rc_field_info(CodeGen* gen, Node* struct_decl, RcFieldInfo** out_fields) {
    int          count  = 0;
    RcFieldInfo* fields = NULL;
    for (int f = 0; f < struct_decl->as.struct_decl.fields.count; f++) {
        Node* field = struct_decl->as.struct_decl.fields.nodes[f];
        if (!field->as.field.type || !type_node_has_rc(gen, field->as.field.type))
            continue;
        count++;
        fields            = xrealloc(fields, count * sizeof(RcFieldInfo));
        RcFieldInfo* info = &fields[count - 1];
        info->field_name  = xstrdup(field->as.field.name);
        if (field->as.field.type->type == NODE_IDENT) {
            info->type_name = xstrdup(field->as.field.type->as.ident.name);
            info->is_enum   = is_enum_type_name(gen, field->as.field.type->as.ident.name);
        } else if (field->as.field.type->type == NODE_GENERIC_TYPE) {
            Node*  gtype     = field->as.field.type;
            int    arg_count = gtype->as.generic_type.type_args.count;
            Type** args      = xmalloc(arg_count * sizeof(Type*));
            for (int a = 0; a < arg_count; a++) {
                args[a] = type_from_node(gtype->as.generic_type.type_args.nodes[a]);
            }
            info->type_name =
                type_mangle_generic(gtype->as.generic_type.base_name, args, arg_count);
            free(args);
            info->is_enum = 0;
        } else {
            info->type_name = NULL;
            info->is_enum   = 0;
        }
    }
    *out_fields = fields;
    return count;
}

// Check if a field type needs a type-specific __rc_dec_TypeName (has Drop or RC fields)
static int field_needs_custom_dec(CodeGen* gen, const char* field_type_name, Node* ast) {
    if (!field_type_name)
        return 0;
    if (struct_implements_drop(gen, field_type_name))
        return 1;
    // Check if field type has RC fields (scan non-generic structs)
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type == NODE_STRUCT_DECL && decl->as.struct_decl.type_param_count == 0 &&
                strcmp(decl->as.struct_decl.name, field_type_name) == 0) {
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    if (field->as.field.type && type_node_has_rc(gen, field->as.field.type))
                        return 1;
                }
                return 0;
            }
        }
    }
    // Check generic instances (e.g., field type "Box_Inner")
    for (int i = 0; i < gen->checker.instance_count; i++) {
        if (strcmp(gen->checker.instances[i].mangled_name, field_type_name) == 0) {
            Type* t = gen->checker.instances[i].type;
            if (t && t->kind == TYPE_STRUCT && (t->as.struc.has_drop || t->as.struc.has_rc_fields))
                return 1;
            return 0;
        }
    }
    return 0;
}

// Emit forward declarations for __rc_dec_TypeName functions (needed for cross-references)
void emit_struct_rc_dec_forward_decls(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL)
                continue;
            if (decl->as.struct_decl.type_param_count > 0)
                continue;
            const char* sname     = decl->as.struct_decl.name;
            int         needs_dec = struct_implements_drop(gen, sname);
            if (!needs_dec) {
                for (int f = 0; f < decl->as.struct_decl.fields.count; f++) {
                    Node* field = decl->as.struct_decl.fields.nodes[f];
                    if (field->as.field.type && type_node_has_rc(gen, field->as.field.type)) {
                        needs_dec = 1;
                        break;
                    }
                }
            }
            if (needs_dec) {
                emit(gen, "static inline void __rc_dec_%s(%s* ptr);\n", sname, sname);
            }
        }
    }
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* info = &gen->checker.instances[i];
        Type*            t    = info->type;
        if (!t || t->kind != TYPE_STRUCT)
            continue;
        if (struct_implements_drop(gen, info->base_name) || t->as.struc.has_rc_fields) {
            emit(gen, "static inline void __rc_dec_%s(%s* ptr);\n", info->mangled_name,
                 info->mangled_name);
        }
    }
    emit(gen, "\n");
}

// Emit push, pop, insert, remove, swap_remove, clear, reserve, shrink_to_fit, contains, sort,
// first, and last method implementations for each Vec type instance
void emit_vec_methods(CodeGen* gen) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst        = &gen->checker.vecs[i];
        Type*        elem_type   = inst->elem_type;
        const char*  elem_tname  = type_mangle_name(elem_type);
        int          elem_is_ptr = (elem_type->kind == TYPE_STRUCT || elem_type->kind == TYPE_VEC);

        // Push
        emit(gen, "static inline void __Vec_%s_push(__Vec_%s* self, ", elem_tname, elem_tname);
        emit_resolved_type(gen, elem_type);
        emit(gen, " value) {\n");
        emit(gen, "    if (self->count == self->capacity) {\n");
        emit(gen, "        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;\n");
        emit(gen, "        self->data = realloc(self->data, new_cap * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "        self->capacity = new_cap;\n");
        emit(gen, "    }\n");
        emit(gen, "    self->data[self->count] = value;\n");
        emit(gen, "    self->count++;\n");
        emit(gen, "}\n\n");

        // Insert
        emit(gen, "static inline void __Vec_%s_insert(__Vec_%s* self, int64_t index, ", elem_tname,
             elem_tname);
        emit_resolved_type(gen, elem_type);
        emit(gen, " value) {\n");
        emit(gen, "    if (index < 0 || index > self->count) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: Vec insert index %%lld out of bounds "
                  "(count=%%lld)\\n\", (long long)index, (long long)self->count);\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "    if (self->count == self->capacity) {\n");
        emit(gen, "        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;\n");
        emit(gen, "        self->data = realloc(self->data, new_cap * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "        self->capacity = new_cap;\n");
        emit(gen, "    }\n");
        emit(gen, "    if (index < self->count) {\n");
        emit(gen, "        memmove(&self->data[index + 1], &self->data[index], "
                  "(size_t)(self->count - index) * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "    }\n");
        emit(gen, "    self->data[index] = value;\n");
        emit(gen, "    self->count++;\n");
        emit(gen, "}\n\n");

        // Reserve
        emit(gen, "static inline void __Vec_%s_reserve(__Vec_%s* self, int64_t additional) {\n",
             elem_tname, elem_tname);
        emit(gen, "    if (additional <= 0) {\n");
        emit(gen, "        return;\n");
        emit(gen, "    }\n");
        emit(gen, "    int64_t required = self->count + additional;\n");
        emit(gen, "    if (required > self->capacity) {\n");
        emit(gen, "        self->data = realloc(self->data, required * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "        self->capacity = required;\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");

        // Contains
        if (type_supports_vec_contains(elem_type)) {
            emit(gen, "static inline bool __Vec_%s_contains(__Vec_%s* self, ", elem_tname,
                 elem_tname);
            emit_resolved_type(gen, elem_type);
            emit(gen, " value) {\n");
            emit(gen, "    for (int64_t i = 0; i < self->count; i++) {\n");
            if (elem_type->kind == TYPE_STRING) {
                emit(gen, "        const char* item = self->data[i];\n");
                emit(gen, "        if ((item == NULL && value == NULL) ||\n");
                emit(gen, "            (item != NULL && value != NULL && strcmp(item, value) == "
                          "0)) {\n");
                emit(gen, "            return true;\n");
                emit(gen, "        }\n");
            } else {
                emit(gen, "        if (self->data[i] == value) {\n");
                emit(gen, "            return true;\n");
                emit(gen, "        }\n");
            }
            emit(gen, "    }\n");
            emit(gen, "    return false;\n");
            emit(gen, "}\n\n");
        }

        // Sort (ascending)
        if (type_supports_vec_sort(elem_type)) {
            emit(gen, "static int __Vec_%s_sort_cmp(const void* a, const void* b) {\n", elem_tname);
            emit(gen, "    const ");
            emit_resolved_type(gen, elem_type);
            emit(gen, "* lhs = (const ");
            emit_resolved_type(gen, elem_type);
            emit(gen, "*)a;\n");
            emit(gen, "    const ");
            emit_resolved_type(gen, elem_type);
            emit(gen, "* rhs = (const ");
            emit_resolved_type(gen, elem_type);
            emit(gen, "*)b;\n");
            if (elem_type->kind == TYPE_STRING) {
                emit(gen, "    if (*lhs == NULL && *rhs == NULL) {\n");
                emit(gen, "        return 0;\n");
                emit(gen, "    }\n");
                emit(gen, "    if (*lhs == NULL) {\n");
                emit(gen, "        return -1;\n");
                emit(gen, "    }\n");
                emit(gen, "    if (*rhs == NULL) {\n");
                emit(gen, "        return 1;\n");
                emit(gen, "    }\n");
                emit(gen, "    return strcmp(*lhs, *rhs);\n");
            } else {
                emit(gen, "    if (*lhs < *rhs) {\n");
                emit(gen, "        return -1;\n");
                emit(gen, "    }\n");
                emit(gen, "    if (*lhs > *rhs) {\n");
                emit(gen, "        return 1;\n");
                emit(gen, "    }\n");
                emit(gen, "    return 0;\n");
            }
            emit(gen, "}\n\n");

            emit(gen, "static inline void __Vec_%s_sort(__Vec_%s* self) {\n", elem_tname,
                 elem_tname);
            emit(gen, "    if (self->count <= 1) {\n");
            emit(gen, "        return;\n");
            emit(gen, "    }\n");
            emit(gen, "    qsort(self->data, (size_t)self->count, sizeof(");
            emit_resolved_type(gen, elem_type);
            emit(gen, "), __Vec_%s_sort_cmp);\n", elem_tname);
            emit(gen, "}\n\n");
        }

        // Pop
        emit(gen, "static inline ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " __Vec_%s_pop(__Vec_%s* self) {\n", elem_tname, elem_tname);
        emit(gen, "    if (self->count == 0) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: pop from empty Vec\\n\");\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "    self->count--;\n");
        emit(gen, "    return self->data[self->count];\n");
        emit(gen, "}\n\n");

        // Remove
        emit(gen, "static inline ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " __Vec_%s_remove(__Vec_%s* self, int64_t index) {\n", elem_tname, elem_tname);
        emit(gen, "    if (index < 0 || index >= self->count) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: Vec remove index %%lld out of bounds "
                  "(count=%%lld)\\n\", (long long)index, (long long)self->count);\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "    ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " removed = self->data[index];\n");
        emit(gen, "    if (index < self->count - 1) {\n");
        emit(gen, "        memmove(&self->data[index], &self->data[index + 1], "
                  "(size_t)(self->count - index - 1) * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "    }\n");
        emit(gen, "    self->count--;\n");
        emit(gen, "    return removed;\n");
        emit(gen, "}\n\n");

        // Swap remove
        emit(gen, "static inline ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " __Vec_%s_swap_remove(__Vec_%s* self, int64_t index) {\n", elem_tname,
             elem_tname);
        emit(gen, "    if (index < 0 || index >= self->count) {\n");
        emit(gen, "        fprintf(stderr, \"Panic: Vec swap_remove index %%lld out of bounds "
                  "(count=%%lld)\\n\", (long long)index, (long long)self->count);\n");
        emit(gen, "        exit(1);\n");
        emit(gen, "    }\n");
        emit(gen, "    ");
        emit_resolved_type(gen, elem_type);
        emit(gen, " removed = self->data[index];\n");
        emit(gen, "    self->count--;\n");
        emit(gen, "    if (index < self->count) {\n");
        emit(gen, "        self->data[index] = self->data[self->count];\n");
        emit(gen, "    }\n");
        emit(gen, "    return removed;\n");
        emit(gen, "}\n\n");

        // Clear
        emit(gen, "static inline void __Vec_%s_clear(__Vec_%s* self) {\n", elem_tname, elem_tname);
        if (elem_is_ptr) {
            emit(gen, "    for (int64_t i = 0; i < self->count; i++) {\n");
            if (elem_type->kind == TYPE_VEC) {
                emit(gen, "        __rc_dec_Vec_%s(self->data[i]);\n",
                     type_mangle_name(elem_type->as.vec.elem));
            } else if (elem_type->kind == TYPE_STRUCT &&
                       (elem_type->as.struc.has_drop || elem_type->as.struc.has_rc_fields)) {
                emit(gen, "        __rc_dec_%s(self->data[i]);\n", elem_type->as.struc.name);
            } else {
                emit(gen, "        __rc_dec(self->data[i]);\n");
            }
            emit(gen, "    }\n");
        }
        emit(gen, "    self->count = 0;\n");
        emit(gen, "}\n\n");

        // Shrink to fit
        emit(gen, "static inline void __Vec_%s_shrink_to_fit(__Vec_%s* self) {\n", elem_tname,
             elem_tname);
        emit(gen, "    if (self->capacity > self->count) {\n");
        emit(gen, "        if (self->count == 0) {\n");
        emit(gen, "            free(self->data);\n");
        emit(gen, "            self->data = NULL;\n");
        emit(gen, "            self->capacity = 0;\n");
        emit(gen, "            return;\n");
        emit(gen, "        }\n");
        emit(gen, "        self->data = realloc(self->data, self->count * sizeof(");
        emit_resolved_type(gen, elem_type);
        emit(gen, "));\n");
        emit(gen, "        self->capacity = self->count;\n");
        emit(gen, "    }\n");
        emit(gen, "}\n\n");

        // First/Last — only emit if Option<elem_type> was instantiated (i.e. first/last used)
        const char* option_tname = type_mangle_name(elem_type);
        char        option_mangled[256];
        snprintf(option_mangled, sizeof(option_mangled), "Option_%s", option_tname);
        int has_option = 0;
        for (int gi = 0; gi < gen->checker.instance_count; gi++) {
            if (strcmp(gen->checker.instances[gi].mangled_name, option_mangled) == 0) {
                has_option = 1;
                break;
            }
        }
        if (has_option) {
            // First — returns Option<T>, Some(v[0]) if non-empty, None if empty
            emit(gen, "static inline Option_%s __Vec_%s_first(__Vec_%s* self) {\n", option_tname,
                 elem_tname, elem_tname);
            emit(gen, "    if (self->count == 0) {\n");
            emit(gen, "        return (Option_%s){.tag = Option_%s_None};\n", option_tname,
                 option_tname);
            emit(gen, "    }\n");
            if (elem_is_ptr) {
                emit(gen, "    __rc_inc(self->data[0]);\n");
            }
            emit(gen,
                 "    return (Option_%s){.tag = Option_%s_Some, .Some = {.f0 = "
                 "self->data[0]}};\n",
                 option_tname, option_tname);
            emit(gen, "}\n\n");

            // Last — returns Option<T>, Some(v[count-1]) if non-empty, None if empty
            emit(gen, "static inline Option_%s __Vec_%s_last(__Vec_%s* self) {\n", option_tname,
                 elem_tname, elem_tname);
            emit(gen, "    if (self->count == 0) {\n");
            emit(gen, "        return (Option_%s){.tag = Option_%s_None};\n", option_tname,
                 option_tname);
            emit(gen, "    }\n");
            if (elem_is_ptr) {
                emit(gen, "    __rc_inc(self->data[self->count - 1]);\n");
            }
            emit(gen,
                 "    return (Option_%s){.tag = Option_%s_Some, "
                 ".Some = {.f0 = self->data[self->count - 1]}};\n",
                 option_tname, option_tname);
            emit(gen, "}\n\n");
        }
    }
}

// Emit __rc_dec_Vec_T functions that free Vec elements and the Vec itself
void emit_vec_rc_dec(CodeGen* gen) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst       = &gen->checker.vecs[i];
        const char*  elem_tname = type_mangle_name(inst->elem_type);
        emit(gen, "static inline void __rc_dec_Vec_%s(__Vec_%s* ptr);\n", elem_tname, elem_tname);
    }

    // Emit __rc_dec_Vec_* definitions
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst        = &gen->checker.vecs[i];
        Type*        elem_type   = inst->elem_type;
        const char*  elem_tname  = type_mangle_name(elem_type);
        int          elem_is_ptr = (elem_type->kind == TYPE_STRUCT || elem_type->kind == TYPE_VEC);

        emit(gen, "static inline void __rc_dec_Vec_%s(__Vec_%s* ptr) {\n", elem_tname, elem_tname);
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
        emit(gen, "    if (--h->refcount == 0) {\n");
        if (elem_is_ptr) {
            emit(gen, "        for (int64_t i = 0; i < ptr->count; i++) {\n");
            if (elem_type->kind == TYPE_VEC) {
                emit(gen, "            __rc_dec_Vec_%s(ptr->data[i]);\n",
                     type_mangle_name(elem_type->as.vec.elem));
            } else if (elem_type->kind == TYPE_STRUCT &&
                       (elem_type->as.struc.has_drop || elem_type->as.struc.has_rc_fields)) {
                emit(gen, "            __rc_dec_%s(ptr->data[i]);\n", elem_type->as.struc.name);
            } else {
                emit(gen, "            __rc_dec(ptr->data[i]);\n");
            }
            emit(gen, "        }\n");
        }
        emit(gen, "        free(ptr->data);\n");
        if (gen->rc.debug) {
            emit(gen, "        fprintf(stderr, \"RC_FREE: %%p\\n\", (void*)ptr);\n");
        }
        emit(gen, "        free(h);\n");
        if (gen->rc.debug) {
            emit(gen, "    } else {\n");
            emit(gen, "        fprintf(stderr, \"RC_DEC: %%p (rc=%%zu)\\n\", (void*)ptr, "
                      "h->refcount);\n");
        }
        emit(gen, "    }\n");
        emit(gen, "}\n\n");
    }
}

// Emit __rc_dec_TypeName definitions for structs with Drop or RC-managed fields
void emit_struct_rc_dec(CodeGen* gen, Node* ast) {
    // Emit type-specific __rc_dec_TypeName functions for structs with Drop or RC fields
    // Non-generic structs: scan modules for struct decls with resolved types
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (decl->type != NODE_STRUCT_DECL)
                continue;
            if (decl->as.struct_decl.type_param_count > 0)
                continue; // Skip generic templates

            const char* sname    = decl->as.struct_decl.name;
            int         has_drop = struct_implements_drop(gen, sname);

            RcFieldInfo* rc_fields      = NULL;
            int          rc_field_count = collect_rc_field_info(gen, decl, &rc_fields);

            if (!has_drop && rc_field_count == 0)
                continue;

            emit(gen, "static inline void __rc_dec_%s(%s* ptr) {\n", sname, sname);
            emit(gen, "    if (!ptr) return;\n");
            emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
            emit(gen, "    if (--h->refcount == 0) {\n");
            if (has_drop) {
                emit(gen, "        %s_drop(ptr);\n", sname);
            }
            for (int f = 0; f < rc_field_count; f++) {
                const char* field_tname = rc_fields[f].type_name;
                if (rc_fields[f].is_enum) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", field_tname,
                         rc_fields[f].field_name);
                } else if (field_needs_custom_dec(gen, field_tname, ast)) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", field_tname,
                         rc_fields[f].field_name);
                } else {
                    emit(gen, "        __rc_dec(ptr->%s);\n", rc_fields[f].field_name);
                }
            }
            if (gen->rc.debug) {
                emit(gen, "        fprintf(stderr, \"RC_FREE: %%p\\n\", (void*)ptr);\n");
            }
            emit(gen, "        free(h);\n");
            if (gen->rc.debug) {
                emit(gen, "    } else {\n");
                emit(gen, "        fprintf(stderr, \"RC_DEC: %%p (rc=%%zu)\\n\", (void*)ptr, "
                          "h->refcount);\n");
            }
            emit(gen, "    }\n");
            emit(gen, "}\n\n");

            for (int f = 0; f < rc_field_count; f++) {
                free(rc_fields[f].field_name);
                free(rc_fields[f].type_name);
            }
            free(rc_fields);
        }
    }

    // Generic instances: emit __rc_dec_MangledName for those with Drop or RC fields
    for (int i = 0; i < gen->checker.instance_count; i++) {
        GenericInstance* info = &gen->checker.instances[i];
        Type*            t    = info->type;
        if (!t || t->kind != TYPE_STRUCT)
            continue;

        int has_drop      = struct_implements_drop(gen, info->base_name);
        int has_rc_fields = t->as.struc.has_rc_fields;

        if (!has_drop && !has_rc_fields)
            continue;

        const char* mname = info->mangled_name;
        emit(gen, "static inline void __rc_dec_%s(%s* ptr) {\n", mname, mname);
        emit(gen, "    if (!ptr) return;\n");
        emit(gen, "    __RcHeader* h = (__RcHeader*)ptr - 1;\n");
        emit(gen, "    if (--h->refcount == 0) {\n");
        if (has_drop) {
            emit(gen, "        %s_drop(ptr);\n", mname);
        }
        for (int f = 0; f < t->as.struc.field_count; f++) {
            Type* ft = t->as.struc.field_types[f];
            if (ft && ft->kind == TYPE_STRUCT) {
                if (ft->as.struc.has_drop || ft->as.struc.has_rc_fields) {
                    emit(gen, "        __rc_dec_%s(ptr->%s);\n", ft->as.struc.name,
                         t->as.struc.field_names[f]);
                } else {
                    emit(gen, "        __rc_dec(ptr->%s);\n", t->as.struc.field_names[f]);
                }
            } else if (ft && ft->kind == TYPE_VEC) {
                emit(gen, "        __rc_dec_Vec_%s(ptr->%s);\n", type_mangle_name(ft->as.vec.elem),
                     t->as.struc.field_names[f]);
            }
        }
        if (gen->rc.debug) {
            emit(gen, "        fprintf(stderr, \"RC_FREE: %%p\\n\", (void*)ptr);\n");
        }
        emit(gen, "        free(h);\n");
        if (gen->rc.debug) {
            emit(gen, "    } else {\n");
            emit(gen, "        fprintf(stderr, \"RC_DEC: %%p (rc=%%zu)\\n\", (void*)ptr, "
                      "h->refcount);\n");
        }
        emit(gen, "    }\n");
        emit(gen, "}\n\n");
    }
}
