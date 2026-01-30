#include "check_decl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "check_statement.h"
#include "checker_util.h"

void check_decl(Checker* checker, Node* node) {
    if (!node)
        return;

    switch (node->type) {
    case NODE_FUNC_DECL: {
        const char* name          = node->as.func_decl.name;
        const char* receiver_type = node->as.func_decl.receiver_type;
        int         is_method     = (receiver_type != NULL);

        // For methods, use mangled name: StructName_methodName
        char* mangled_name = NULL;
        if (is_method) {
            size_t len  = strlen(receiver_type) + 1 + strlen(name) + 1;
            mangled_name = malloc(len);
            if (!mangled_name) {
                check_error(checker, node->line, node->column, "Out of memory");
                return;
            }
            snprintf(mangled_name, len, "%s_%s", receiver_type, name);
        } else {
            mangled_name = strdup(name);
            if (!mangled_name) {
                check_error(checker, node->line, node->column, "Out of memory");
                return;
            }
        }

        // Check for redefinition
        if (checker_lookup(checker, mangled_name)) {
            check_error(checker, node->line, node->column, "Redefinition of '%s'", mangled_name);
            free(mangled_name);
            return;
        }

        // Build function type
        int    param_count = node->as.func_decl.params.count;
        Type** param_types = NULL;
        if (param_count > 0) {
            param_types = malloc(param_count * sizeof(Type*));
            if (!param_types) {
                check_error(checker, node->line, node->column, "Out of memory");
                free(mangled_name);
                return;
            }
        }

        Type* return_type = type_void;
        if (node->as.func_decl.return_type) {
            return_type = resolve_type(checker, node->as.func_decl.return_type);
        }

        // Pre-declare function for recursion
        Type* func_type = type_func(param_types, param_count, return_type);
        checker_define(checker, mangled_name, SYM_FUNC, func_type, 0);

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

                char** new_names = realloc(st->as.struc.method_names, (n + 1) * sizeof(char*));
                if (!new_names) {
                    check_error(checker, node->line, node->column, "Out of memory");
                    free(mangled_name);
                    return;
                }
                st->as.struc.method_names = new_names;

                Type** new_types = realloc(st->as.struc.method_types, (n + 1) * sizeof(Type*));
                if (!new_types) {
                    check_error(checker, node->line, node->column, "Out of memory");
                    free(mangled_name);
                    return;
                }
                st->as.struc.method_types = new_types;

                int* new_const = realloc(st->as.struc.method_is_const, (n + 1) * sizeof(int));
                if (!new_const) {
                    check_error(checker, node->line, node->column, "Out of memory");
                    free(mangled_name);
                    return;
                }
                st->as.struc.method_is_const = new_const;

                char* method_name = strdup(name);
                if (!method_name) {
                    check_error(checker, node->line, node->column, "Out of memory");
                    free(mangled_name);
                    return;
                }

                st->as.struc.method_names[n]    = method_name;
                st->as.struc.method_types[n]    = func_type;
                st->as.struc.method_is_const[n] = node->as.func_decl.receiver_is_const;
                st->as.struc.method_count       = n + 1;
            }
        }

        // Enter function scope
        checker_push_scope(checker);
        Type* old_return             = checker->current_func_return;
        checker->current_func_return = return_type;

        // For methods, inject 'self' into scope
        if (is_method) {
            Symbol* struct_sym = checker_lookup(checker, receiver_type);
            if (struct_sym && struct_sym->kind == SYM_TYPE) {
                Type* self_type = type_pointer(struct_sym->type);
                checker_define(checker, "self", SYM_VAR, self_type,
                               node->as.func_decl.receiver_is_const);
            }
        }

        // Define parameters
        for (int i = 0; i < param_count; i++) {
            Node* param = node->as.func_decl.params.nodes[i];
            Type* ptype = type_void;
            if (param->as.param.type) {
                ptype = resolve_type(checker, param->as.param.type);
            }
            param_types[i] = ptype;

            if (!checker_define(checker, param->as.param.name, SYM_VAR, ptype, 0)) {
                check_error(checker, param->line, param->column, "Duplicate parameter name '%s'",
                            param->as.param.name);
            }
        }

        // Check body
        if (node->as.func_decl.body) {
            // Body is a block, but we already pushed scope for params
            // So just check the statements directly
            Node* body = node->as.func_decl.body;
            for (int i = 0; i < body->as.block.stmts.count; i++) {
                check_statement(checker, body->as.block.stmts.nodes[i]);
            }
        }

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
        struct_type->as.struc.field_names = malloc(field_count * sizeof(char*));
        struct_type->as.struc.field_types = malloc(field_count * sizeof(Type*));

        for (int i = 0; i < field_count; i++) {
            Node* field                          = node->as.struct_decl.fields.nodes[i];
            struct_type->as.struc.field_names[i] = strdup(field->as.field.name);
            struct_type->as.struc.field_types[i] = resolve_type(checker, field->as.field.type);
        }

        checker_define(checker, name, SYM_TYPE, struct_type, 0);
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
        enum_type->as.enm.value_names = malloc(value_count * sizeof(char*));

        checker_define(checker, name, SYM_TYPE, enum_type, 0);

        // Define enum values as constants
        for (int i = 0; i < value_count; i++) {
            Node* val                        = node->as.enum_decl.values.nodes[i];
            enum_type->as.enm.value_names[i] = strdup(val->as.ident.name);
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
