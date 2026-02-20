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

// Return the cleanup function name for a type, or NULL if none needed.
// Returns a malloc'd string. Caller must free.
char* get_cleanup_func_for_type(Type* t) {
    if (t && t->kind == TYPE_STRUCT && (t->as.struc.has_drop || t->as.struc.has_rc_fields)) {
        size_t len = strlen("__") + strlen(t->as.struc.name) + strlen("_cleanup") + 1;
        char*  buf = xmalloc(len);
        snprintf(buf, len, "__%s_cleanup", t->as.struc.name);
        return buf;
    }
    if (t && t->kind == TYPE_STRINGBUILDER) {
        return xstrdup("__StringBuilder_cleanup");
    }
    if (t && t->kind == TYPE_VEC) {
        const char* elem_tname = type_mangle_name(t->as.vec.elem);
        size_t      len        = strlen("__Vec_") + strlen(elem_tname) + strlen("_cleanup") + 1;
        char*       buf        = xmalloc(len);
        snprintf(buf, len, "__Vec_%s_cleanup", elem_tname);
        return buf;
    }
    return NULL;
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
void rc_push_var(CodeGen* gen, const char* name, Type* type) {
    VEC_GROW(gen->rc.vars, gen->rc.count, gen->rc.capacity);
    gen->rc.vars[gen->rc.count].name        = xstrdup(name);
    gen->rc.vars[gen->rc.count].type        = type;
    gen->rc.vars[gen->rc.count].scope_depth = gen->rc.depth;
    gen->rc.count++;
}

// Emit the appropriate dec call for an RC variable (enum value types need type-specific dec)
static void emit_rc_dec_var(CodeGen* gen, RcVar* var) {
    Type* t = var->type;
    if (t && t->kind == TYPE_ENUM && t->as.enm.has_rc_fields) {
        emit(gen, "__rc_dec_%s(%s)", t->as.enm.name, var->name);
    } else if (t && t->kind == TYPE_STRING) {
        emit(gen, "__rc_dec((void*)%s)", var->name);
    } else {
        emit(gen, "__rc_dec(%s)", var->name);
    }
}

// Emit dec for vars at the given depth, remove them from list
void rc_cleanup_scope(CodeGen* gen, int depth) {
    int dst = 0;
    for (int i = 0; i < gen->rc.count; i++) {
        if (gen->rc.vars[i].scope_depth == depth) {
            emit_indent(gen);
            emit_rc_dec_var(gen, &gen->rc.vars[i]);
            emit(gen, ";\n");
            free(gen->rc.vars[i].name);
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
        emit_rc_dec_var(gen, &gen->rc.vars[i]);
        emit(gen, ";\n");
    }
}

// Free and reset the RC var list (at function boundary)
void rc_clear_all(CodeGen* gen) {
    for (int i = 0; i < gen->rc.count; i++) {
        free(gen->rc.vars[i].name);
    }
    gen->rc.count = 0;
    gen->rc.depth = 0;
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
                // Check if this field resolves to a string (directly or via substitution)
                int is_str = 0;
                if (tnode->type == NODE_IDENT) {
                    if (strcmp(tnode->as.ident.name, "string") == 0) {
                        is_str = 1;
                    } else {
                        Type* resolved = subst_lookup(gen, tnode->as.ident.name);
                        is_str         = (resolved && resolved->kind == TYPE_STRING);
                    }
                }
                if (is_str) {
                    emit(gen, "        __rc_%s((void*)v.%.*s.f%d);\n", op,
                         var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                } else {
                    emit(gen, "        __rc_%s(v.%.*s.f%d);\n", op,
                         var->as.enum_variant.name_length, var->as.enum_variant.name, t);
                }
            }
        }
        emit(gen, "        break;\n");
    }
    emit(gen, "    default: break;\n");
    emit(gen, "    }\n");
    emit(gen, "}\n\n");
}

// Return whether a declaration is a non-generic enum declaration.
static int is_non_generic_enum_decl(Node* decl) {
    return decl && decl->type == NODE_ENUM_DECL && decl->as.enum_decl.type_param_count == 0;
}

// Emit forward declarations for __rc_inc_Name and __rc_dec_Name.
static void emit_enum_rc_forward_decls_for_name(CodeGen* gen, const char* name) {
    emit(gen, "static inline void __rc_inc_%s(%s v);\n", name, name);
    emit(gen, "static inline void __rc_dec_%s(%s v);\n", name, name);
}

// Emit enum RC forward declarations for non-generic enum declarations.
static void emit_non_generic_enum_rc_forward_decls(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (!is_non_generic_enum_decl(decl))
                continue;
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_rc_fields(gen, ename))
                continue;
            emit_enum_rc_forward_decls_for_name(gen, ename);
        }
    }
}

// Emit enum RC forward declarations for generic enum instances.
static void emit_generic_enum_rc_forward_decls(CodeGen* gen) {
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_rc_fields(gen, info->mangled_name))
            continue;
        emit_enum_rc_forward_decls_for_name(gen, info->mangled_name);
    }
}

// Emit enum RC function definitions for non-generic enum declarations.
static void emit_non_generic_enum_rc_defs(CodeGen* gen, Node* ast) {
    for (int m = 0; m < ast->as.program.modules.count; m++) {
        Node* mod = ast->as.program.modules.nodes[m];
        if (!mod || mod->type != NODE_MODULE)
            continue;
        for (int i = 0; i < mod->as.module.decls.count; i++) {
            Node* decl = mod->as.module.decls.nodes[i];
            if (!is_non_generic_enum_decl(decl))
                continue;
            const char* ename = decl->as.enum_decl.name;
            if (!enum_has_rc_fields(gen, ename))
                continue;
            emit_enum_rc_func(gen, ename, decl, 1); // __rc_inc
            emit_enum_rc_func(gen, ename, decl, 0); // __rc_dec
        }
    }
}

// Emit enum RC function definitions for one generic enum instance with substitution bound.
static void emit_generic_enum_rc_defs_for_instance(CodeGen* gen, GenericInstance* info,
                                                   Node* tmpl) {
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

// Emit enum RC function definitions for generic enum instances.
static void emit_generic_enum_rc_defs(CodeGen* gen, Node* ast) {
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        GenericInstance* info = &gen->checker.instances[gi];
        if (info->type->kind != TYPE_ENUM)
            continue;
        if (!enum_has_rc_fields(gen, info->mangled_name))
            continue;
        Node* tmpl = find_generic_enum_decl(ast, info->base_name);
        if (!tmpl)
            continue;

        emit_generic_enum_rc_defs_for_instance(gen, info, tmpl);
    }
}

// Emit __rc_inc_EnumName/__rc_dec_EnumName helpers for enums with RC-managed fields
void emit_enum_rc_helpers(CodeGen* gen, Node* ast) {
    emit_non_generic_enum_rc_forward_decls(gen, ast);
    emit_generic_enum_rc_forward_decls(gen);
    if (gen->enums.count > 0)
        emit(gen, "\n");

    emit_non_generic_enum_rc_defs(gen, ast);
    emit_generic_enum_rc_defs(gen, ast);
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

typedef struct {
    Type*       elem_type;
    const char* elem_tname;
    int         elem_is_ptr;
    int         elem_is_rc_enum;
} VecMethodEmitCtx;

static void emit_vec_elem_inc_stmt(CodeGen* gen, const VecMethodEmitCtx* ctx, const char* value) {
    if (ctx->elem_is_ptr) {
        if (ctx->elem_type->kind == TYPE_STRING) {
            emit(gen, "    __rc_inc((void*)%s);\n", value);
        } else {
            emit(gen, "    __rc_inc(%s);\n", value);
        }
    } else if (ctx->elem_is_rc_enum) {
        emit(gen, "    __rc_inc_%s(%s);\n", ctx->elem_type->as.enm.name, value);
    }
}

static void emit_vec_push_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    // Vec<string> push is provided by whist_runtime.h.
    if (ctx->elem_type->kind == TYPE_STRING) {
        return;
    }

    emit(gen, "static inline void __Vec_%s_push(__Vec_%s* self, ", ctx->elem_tname,
         ctx->elem_tname);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " value) {\n");
    emit(gen, "    if (self->count == self->capacity) {\n");
    emit(gen, "        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;\n");
    emit(gen, "        self->data = realloc(self->data, new_cap * sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "));\n");
    emit(gen, "        self->capacity = new_cap;\n");
    emit(gen, "    }\n");
    emit_vec_elem_inc_stmt(gen, ctx, "value");
    emit(gen, "    self->data[self->count] = value;\n");
    emit(gen, "    self->count++;\n");
    emit(gen, "}\n\n");
}

static void emit_vec_insert_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    emit(gen, "static inline void __Vec_%s_insert(__Vec_%s* self, int64_t index, ", ctx->elem_tname,
         ctx->elem_tname);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " value) {\n");
    emit(gen, "    if (index < 0 || index > self->count) {\n");
    emit(gen, "        fprintf(stderr, \"Panic: Vec insert index %%lld out of bounds "
              "(count=%%lld)\\n\", (long long)index, (long long)self->count);\n");
    emit(gen, "        exit(1);\n");
    emit(gen, "    }\n");
    emit(gen, "    if (self->count == self->capacity) {\n");
    emit(gen, "        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;\n");
    emit(gen, "        self->data = realloc(self->data, new_cap * sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "));\n");
    emit(gen, "        self->capacity = new_cap;\n");
    emit(gen, "    }\n");
    emit(gen, "    if (index < self->count) {\n");
    emit(gen, "        memmove(&self->data[index + 1], &self->data[index], "
              "(size_t)(self->count - index) * sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "));\n");
    emit(gen, "    }\n");
    emit_vec_elem_inc_stmt(gen, ctx, "value");
    emit(gen, "    self->data[index] = value;\n");
    emit(gen, "    self->count++;\n");
    emit(gen, "}\n\n");
}

static void emit_vec_reserve_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    emit(gen, "static inline void __Vec_%s_reserve(__Vec_%s* self, int64_t additional) {\n",
         ctx->elem_tname, ctx->elem_tname);
    emit(gen, "    if (additional <= 0) {\n");
    emit(gen, "        return;\n");
    emit(gen, "    }\n");
    emit(gen, "    int64_t required = self->count + additional;\n");
    emit(gen, "    if (required > self->capacity) {\n");
    emit(gen, "        self->data = realloc(self->data, required * sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "));\n");
    emit(gen, "        self->capacity = required;\n");
    emit(gen, "    }\n");
    emit(gen, "}\n\n");
}

static void emit_vec_contains_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    if (!type_supports_vec_contains(ctx->elem_type)) {
        return;
    }

    emit(gen, "static inline bool __Vec_%s_contains(__Vec_%s* self, ", ctx->elem_tname,
         ctx->elem_tname);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " value) {\n");
    emit(gen, "    for (int64_t i = 0; i < self->count; i++) {\n");
    if (ctx->elem_type->kind == TYPE_STRING) {
        emit(gen, "        const char* item = self->data[i];\n");
        emit(gen, "        if ((item == NULL && value == NULL) ||\n");
        emit(gen, "            (item != NULL && value != NULL && strcmp(item, value) == 0)) {\n");
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

static void emit_vec_sort_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    if (!type_supports_vec_sort(ctx->elem_type)) {
        return;
    }

    emit(gen, "static int __Vec_%s_sort_cmp(const void* a, const void* b) {\n", ctx->elem_tname);
    const char* const_prefix = (ctx->elem_type->kind == TYPE_STRING) ? "" : "const ";
    emit(gen, "    %s", const_prefix);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "* lhs = (%s", const_prefix);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "*)a;\n");
    emit(gen, "    %s", const_prefix);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "* rhs = (%s", const_prefix);
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "*)b;\n");
    if (ctx->elem_type->kind == TYPE_STRING) {
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

    emit(gen, "static inline void __Vec_%s_sort(__Vec_%s* self) {\n", ctx->elem_tname,
         ctx->elem_tname);
    emit(gen, "    if (self->count <= 1) {\n");
    emit(gen, "        return;\n");
    emit(gen, "    }\n");
    emit(gen, "    qsort(self->data, (size_t)self->count, sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "), __Vec_%s_sort_cmp);\n", ctx->elem_tname);
    emit(gen, "}\n\n");
}

static void emit_vec_remove_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    emit(gen, "static inline ");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " __Vec_%s_remove(__Vec_%s* self, int64_t index) {\n", ctx->elem_tname,
         ctx->elem_tname);
    emit(gen, "    if (index < 0 || index >= self->count) {\n");
    emit(gen, "        fprintf(stderr, \"Panic: Vec remove index %%lld out of bounds "
              "(count=%%lld)\\n\", (long long)index, (long long)self->count);\n");
    emit(gen, "        exit(1);\n");
    emit(gen, "    }\n");
    emit(gen, "    ");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " removed = self->data[index];\n");
    emit(gen, "    if (index < self->count - 1) {\n");
    emit(gen, "        memmove(&self->data[index], &self->data[index + 1], "
              "(size_t)(self->count - index - 1) * sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "));\n");
    emit(gen, "    }\n");
    emit(gen, "    self->count--;\n");
    emit(gen, "    return removed;\n");
    emit(gen, "}\n\n");
}

static void emit_vec_swap_remove_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    emit(gen, "static inline ");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " __Vec_%s_swap_remove(__Vec_%s* self, int64_t index) {\n", ctx->elem_tname,
         ctx->elem_tname);
    emit(gen, "    if (index < 0 || index >= self->count) {\n");
    emit(gen, "        fprintf(stderr, \"Panic: Vec swap_remove index %%lld out of bounds "
              "(count=%%lld)\\n\", (long long)index, (long long)self->count);\n");
    emit(gen, "        exit(1);\n");
    emit(gen, "    }\n");
    emit(gen, "    ");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, " removed = self->data[index];\n");
    emit(gen, "    self->count--;\n");
    emit(gen, "    if (index < self->count) {\n");
    emit(gen, "        self->data[index] = self->data[self->count];\n");
    emit(gen, "    }\n");
    emit(gen, "    return removed;\n");
    emit(gen, "}\n\n");
}

static void emit_vec_clear_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    emit(gen, "static inline void __Vec_%s_clear(__Vec_%s* self) {\n", ctx->elem_tname,
         ctx->elem_tname);
    if (ctx->elem_is_ptr) {
        emit(gen, "    for (int64_t i = 0; i < self->count; i++) {\n");
        if (ctx->elem_type->kind == TYPE_STRING) {
            emit(gen, "        __rc_dec((void*)self->data[i]);\n");
        } else {
            emit(gen, "        __rc_dec(self->data[i]);\n");
        }
        emit(gen, "    }\n");
    } else if (ctx->elem_is_rc_enum) {
        emit(gen, "    for (int64_t i = 0; i < self->count; i++) {\n");
        emit(gen, "        __rc_dec_%s(self->data[i]);\n", ctx->elem_type->as.enm.name);
        emit(gen, "    }\n");
    }
    emit(gen, "    self->count = 0;\n");
    emit(gen, "}\n\n");
}

static void emit_vec_shrink_to_fit_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    emit(gen, "static inline void __Vec_%s_shrink_to_fit(__Vec_%s* self) {\n", ctx->elem_tname,
         ctx->elem_tname);
    emit(gen, "    if (self->capacity > self->count) {\n");
    emit(gen, "        if (self->count == 0) {\n");
    emit(gen, "            free(self->data);\n");
    emit(gen, "            self->data = NULL;\n");
    emit(gen, "            self->capacity = 0;\n");
    emit(gen, "            return;\n");
    emit(gen, "        }\n");
    emit(gen, "        self->data = realloc(self->data, self->count * sizeof(");
    emit_resolved_type(gen, ctx->elem_type);
    emit(gen, "));\n");
    emit(gen, "        self->capacity = self->count;\n");
    emit(gen, "    }\n");
    emit(gen, "}\n\n");
}

static int vec_has_option_instance(CodeGen* gen, const char* option_mangled) {
    for (int gi = 0; gi < gen->checker.instance_count; gi++) {
        if (strcmp(gen->checker.instances[gi].mangled_name, option_mangled) == 0) {
            return 1;
        }
    }
    return 0;
}

static void emit_vec_option_methods(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    const char* option_tname = type_mangle_name(ctx->elem_type);
    char        option_mangled[256];
    snprintf(option_mangled, sizeof(option_mangled), "Option_%s", option_tname);
    if (!vec_has_option_instance(gen, option_mangled)) {
        return;
    }

    // First — returns Option<T>, Some(v[0]) if non-empty, None if empty.
    emit(gen, "static inline Option_%s __Vec_%s_first(__Vec_%s* self) {\n", option_tname,
         ctx->elem_tname, ctx->elem_tname);
    emit(gen, "    if (self->count == 0) {\n");
    emit(gen, "        return (Option_%s){.tag = Option_%s_None};\n", option_tname, option_tname);
    emit(gen, "    }\n");
    emit_vec_elem_inc_stmt(gen, ctx, "self->data[0]");
    emit(gen,
         "    return (Option_%s){.tag = Option_%s_Some, .Some = {.f0 = "
         "self->data[0]}};\n",
         option_tname, option_tname);
    emit(gen, "}\n\n");

    // Last — returns Option<T>, Some(v[count-1]) if non-empty, None if empty.
    emit(gen, "static inline Option_%s __Vec_%s_last(__Vec_%s* self) {\n", option_tname,
         ctx->elem_tname, ctx->elem_tname);
    emit(gen, "    if (self->count == 0) {\n");
    emit(gen, "        return (Option_%s){.tag = Option_%s_None};\n", option_tname, option_tname);
    emit(gen, "    }\n");
    emit_vec_elem_inc_stmt(gen, ctx, "self->data[self->count - 1]");
    emit(gen,
         "    return (Option_%s){.tag = Option_%s_Some, "
         ".Some = {.f0 = self->data[self->count - 1]}};\n",
         option_tname, option_tname);
    emit(gen, "}\n\n");

    // Pop — returns Option<T>, transfers ownership (no __rc_inc).
    emit(gen, "static inline Option_%s __Vec_%s_pop(__Vec_%s* self) {\n", option_tname,
         ctx->elem_tname, ctx->elem_tname);
    emit(gen, "    if (self->count == 0) {\n");
    emit(gen, "        return (Option_%s){.tag = Option_%s_None};\n", option_tname, option_tname);
    emit(gen, "    }\n");
    emit(gen, "    self->count--;\n");
    emit(gen,
         "    return (Option_%s){.tag = Option_%s_Some, "
         ".Some = {.f0 = self->data[self->count]}};\n",
         option_tname, option_tname);
    emit(gen, "}\n\n");
}

static void emit_vec_eq_method(CodeGen* gen, const VecMethodEmitCtx* ctx) {
    if (!type_supports_equality(ctx->elem_type)) {
        return;
    }

    emit(gen, "static inline bool __Vec_%s_eq(__Vec_%s* a, __Vec_%s* b) {\n", ctx->elem_tname,
         ctx->elem_tname, ctx->elem_tname);
    emit(gen, "    if (a == b) return true;\n");
    emit(gen, "    if (a->count != b->count) return false;\n");
    emit(gen, "    for (int64_t i = 0; i < a->count; i++) {\n");
    if (ctx->elem_type->kind == TYPE_STRING) {
        emit(gen, "        if (strcmp(a->data[i], b->data[i]) != 0) return false;\n");
    } else if (ctx->elem_type->kind == TYPE_STRUCT) {
        emit(gen, "        if (!%s_eq(a->data[i], b->data[i])) return false;\n",
             ctx->elem_type->as.struc.name);
    } else if (ctx->elem_type->kind == TYPE_VEC) {
        const char* sub_tname = type_mangle_name(ctx->elem_type->as.vec.elem);
        emit(gen, "        if (!__Vec_%s_eq(a->data[i], b->data[i])) return false;\n", sub_tname);
    } else {
        emit(gen, "        if (a->data[i] != b->data[i]) return false;\n");
    }
    emit(gen, "    }\n");
    emit(gen, "    return true;\n");
    emit(gen, "}\n\n");
}

// Emit push, pop, insert, remove, swap_remove, clear, reserve, shrink_to_fit, contains, sort,
// first, and last method implementations for each Vec type instance.
void emit_vec_methods(CodeGen* gen) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance*     inst = &gen->checker.vecs[i];
        VecMethodEmitCtx ctx;
        ctx.elem_type  = inst->elem_type;
        ctx.elem_tname = type_mangle_name(ctx.elem_type);
        ctx.elem_is_ptr =
            (ctx.elem_type->kind == TYPE_STRUCT || ctx.elem_type->kind == TYPE_VEC ||
             ctx.elem_type->kind == TYPE_STRING || ctx.elem_type->kind == TYPE_STRINGBUILDER);
        ctx.elem_is_rc_enum =
            (ctx.elem_type->kind == TYPE_ENUM && ctx.elem_type->as.enm.has_rc_fields);

        emit_vec_push_method(gen, &ctx);
        emit_vec_insert_method(gen, &ctx);
        emit_vec_reserve_method(gen, &ctx);
        emit_vec_contains_method(gen, &ctx);
        emit_vec_sort_method(gen, &ctx);
        emit_vec_remove_method(gen, &ctx);
        emit_vec_swap_remove_method(gen, &ctx);
        emit_vec_clear_method(gen, &ctx);
        emit_vec_shrink_to_fit_method(gen, &ctx);
        emit_vec_option_methods(gen, &ctx);
        emit_vec_eq_method(gen, &ctx);
    }
}

// Emit user-defined method implementations for each Vec type instance
void emit_vec_user_methods(CodeGen* gen, Node* ast) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst = &gen->checker.vecs[i];
        if (inst->method_count == 0) {
            continue;
        }

        const char* elem_tname = type_mangle_name(inst->elem_type);

        // Get the Vec GenericDef's methods from AST to get param type nodes
        Node** methods      = NULL;
        int    method_count = 0;
        collect_generic_methods(ast, "Vec", &methods, &method_count);

        for (int m = 0; m < inst->method_count && m < method_count; m++) {
            func_decl_node* fdn = &methods[m]->as.func_decl;

            // Set up substitution: T -> elem_type
            TypeSubstContext subst_ctx;
            char*            tp   = "T";
            subst_ctx.type_params = &tp;
            subst_ctx.type_args   = &inst->elem_type;
            subst_ctx.count       = 1;
            gen->generics.subst   = &subst_ctx;

            // Emit return type
            if (fdn->return_is_const && fdn->return_type) {
                emit(gen, "const ");
            }
            emit_type(gen, fdn->return_type);

            // Method name: __Vec_T_methodname(
            emit(gen, " __Vec_%s_%s(__Vec_%s* self", elem_tname, fdn->name, elem_tname);

            // Other parameters
            for (int p = 0; p < fdn->params.count; p++) {
                emit(gen, ", ");
                Node* param = fdn->params.nodes[p];
                if (param->as.param.is_const) {
                    emit(gen, "const ");
                }
                emit_type_with_name(gen, param->as.param.type, param->as.param.name);
            }
            emit(gen, ") {\n");

            // Clear defer stack for this function
            defer_clear(gen);
            gen->defer.return_type = fdn->return_type;

            // Use per-instantiation cloned body
            Node* method_body = NULL;
            if (inst->method_bodies && m < inst->method_body_count && inst->method_bodies[m]) {
                method_body = inst->method_bodies[m];
            } else {
                method_body = fdn->body;
            }

            gen->out.indent++;

            if (method_body) {
                for (int s = 0; s < method_body->as.block.stmts.count; s++) {
                    emit_stmt(gen, method_body->as.block.stmts.nodes[s]);
                }
            }

            // RC cleanup for implicit void return
            if (gen->rc.count > 0 && method_body) {
                int   sc   = method_body->as.block.stmts.count;
                Node* last = sc > 0 ? method_body->as.block.stmts.nodes[sc - 1] : NULL;
                if (!last || last->type != NODE_RETURN) {
                    rc_cleanup_all(gen, NULL);
                }
            }

            gen->out.indent--;
            emit(gen, "}\n\n");

            // Clear defer stack and RC tracking
            defer_clear(gen);
            rc_clear_all(gen);
            gen->defer.return_type = NULL;
            gen->generics.subst    = NULL;
        }

        free(methods);
    }
}

// Emit __Vec_T_cleanup functions that free Vec elements and data array
void emit_vec_cleanup(CodeGen* gen) {
    for (int i = 0; i < gen->checker.vec_count; i++) {
        VecInstance* inst        = &gen->checker.vecs[i];
        Type*        elem_type   = inst->elem_type;
        const char*  elem_tname  = type_mangle_name(elem_type);
        int          elem_is_ptr = (elem_type->kind == TYPE_STRUCT || elem_type->kind == TYPE_VEC ||
                           elem_type->kind == TYPE_STRINGBUILDER);
        int elem_is_rc_enum = (elem_type->kind == TYPE_ENUM && elem_type->as.enm.has_rc_fields);

        // Vec<string> cleanup is provided by whist_runtime.h
        if (elem_type->kind == TYPE_STRING)
            continue;

        emit(gen, "static inline void __Vec_%s_cleanup(void* raw) {\n", elem_tname);
        emit(gen, "    __Vec_%s* ptr = (__Vec_%s*)raw;\n", elem_tname, elem_tname);
        if (elem_is_ptr) {
            emit(gen, "    for (int64_t i = 0; i < ptr->count; i++) {\n");
            emit(gen, "        __rc_dec(ptr->data[i]);\n");
            emit(gen, "    }\n");
        } else if (elem_is_rc_enum) {
            emit(gen, "    for (int64_t i = 0; i < ptr->count; i++) {\n");
            emit(gen, "        __rc_dec_%s(ptr->data[i]);\n", elem_type->as.enm.name);
            emit(gen, "    }\n");
        }
        emit(gen, "    free(ptr->data);\n");
        emit(gen, "}\n\n");
    }
}

// Emit the common cleanup function prologue for a struct type.
static void emit_struct_cleanup_prologue(CodeGen* gen, const char* type_name) {
    emit(gen, "static inline void __%s_cleanup(void* raw) {\n", type_name);
    emit(gen, "    %s* ptr = (%s*)raw;\n", type_name, type_name);
}

// Emit one RC decrement statement for a non-generic struct field.
static void emit_non_generic_field_cleanup(CodeGen* gen, const RcFieldInfo* field) {
    if (field->is_enum) {
        emit(gen, "    __rc_dec_%s(ptr->%s);\n", field->type_name, field->field_name);
    } else if (field->type_name && strcmp(field->type_name, "string") == 0) {
        emit(gen, "    __rc_dec((void*)ptr->%s);\n", field->field_name);
    } else {
        emit(gen, "    __rc_dec(ptr->%s);\n", field->field_name);
    }
}

// Emit one RC decrement statement for a generic struct instance field.
static void emit_generic_field_cleanup(CodeGen* gen, const char* field_name, Type* field_type) {
    if (!field_type)
        return;

    if (field_type->kind == TYPE_STRUCT || field_type->kind == TYPE_VEC ||
        field_type->kind == TYPE_STRINGBUILDER) {
        emit(gen, "    __rc_dec(ptr->%s);\n", field_name);
    } else if (field_type->kind == TYPE_STRING) {
        emit(gen, "    __rc_dec((void*)ptr->%s);\n", field_name);
    } else if (field_type->kind == TYPE_ENUM && field_type->as.enm.has_rc_fields) {
        emit(gen, "    __rc_dec_%s(ptr->%s);\n", field_type->as.enm.name, field_name);
    }
}

// Free field/type name allocations created by collect_rc_field_info.
static void free_rc_field_info_list(RcFieldInfo* fields, int count) {
    for (int i = 0; i < count; i++) {
        free(fields[i].field_name);
        free(fields[i].type_name);
    }
    free(fields);
}

// Emit cleanup helper for one non-generic struct declaration when needed.
static void emit_non_generic_struct_cleanup(CodeGen* gen, Node* decl) {
    const char* sname    = decl->as.struct_decl.name;
    int         has_drop = struct_implements_drop(gen, sname);

    RcFieldInfo* rc_fields      = NULL;
    int          rc_field_count = collect_rc_field_info(gen, decl, &rc_fields);

    if (!has_drop && rc_field_count == 0) {
        free_rc_field_info_list(rc_fields, rc_field_count);
        return;
    }

    emit_struct_cleanup_prologue(gen, sname);
    if (has_drop) {
        emit(gen, "    %s_drop(ptr);\n", sname);
    }
    for (int f = 0; f < rc_field_count; f++) {
        emit_non_generic_field_cleanup(gen, &rc_fields[f]);
    }
    emit(gen, "}\n\n");

    free_rc_field_info_list(rc_fields, rc_field_count);
}

// Emit cleanup helper for one instantiated generic struct type when needed.
static void emit_generic_struct_cleanup(CodeGen* gen, GenericInstance* info) {
    Type* t = info->type;
    if (!t || t->kind != TYPE_STRUCT)
        return;

    int has_drop      = struct_implements_drop(gen, info->base_name);
    int has_rc_fields = t->as.struc.has_rc_fields;

    if (!has_drop && !has_rc_fields)
        return;

    const char* mname = info->mangled_name;
    emit_struct_cleanup_prologue(gen, mname);
    if (has_drop) {
        emit(gen, "    %s_drop(ptr);\n", mname);
    }
    for (int f = 0; f < t->as.struc.field_count; f++) {
        emit_generic_field_cleanup(gen, t->as.struc.field_names[f], t->as.struc.field_types[f]);
    }
    emit(gen, "}\n\n");
}

// Emit __TypeName_cleanup definitions for structs with Drop or RC-managed fields
void emit_struct_cleanup(CodeGen* gen, Node* ast) {
    // Non-generic structs
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

            emit_non_generic_struct_cleanup(gen, decl);
        }
    }

    // Generic instances
    for (int i = 0; i < gen->checker.instance_count; i++) {
        emit_generic_struct_cleanup(gen, &gen->checker.instances[i]);
    }
}
