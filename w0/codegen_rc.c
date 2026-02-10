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
