#include "check_struct_init.h"

#include <stdlib.h>
#include <string.h>

#include "check_expression.h"
#include "checker_util.h"

Type* check_struct_init(Checker* checker, Node* init, Type* struct_type) {
    if (!struct_type || struct_type->kind != TYPE_STRUCT) {
        check_error(checker, init->line, init->column, "Struct initializer requires a struct type");
        return type_error;
    }

    int  field_count = struct_type->as.struc.field_count;
    int* seen        = calloc(field_count, sizeof(int));
    int  had_error   = 0;

    for (int i = 0; i < init->as.struct_init.fields.count; i++) {
        Node* field = init->as.struct_init.fields.nodes[i];
        if (!field || field->type != NODE_FIELD_INIT) {
            continue;
        }

        const char* field_name  = field->as.field_init.name;
        int         field_index = -1;

        for (int j = 0; j < field_count; j++) {
            if (strcmp(struct_type->as.struc.field_names[j], field_name) == 0) {
                field_index = j;
                break;
            }
        }

        if (field_index < 0) {
            check_error(checker, field->line, field->column, "Struct '%s' has no field '%s'",
                        struct_type->as.struc.name, field_name);
            had_error = 1;
            continue;
        }

        if (seen[field_index]) {
            check_error(checker, field->line, field->column, "Duplicate initializer for field '%s'",
                        field_name);
            had_error = 1;
            continue;
        }

        seen[field_index] = 1;

        Type* value_type = check_expression(checker, field->as.field_init.value);
        Type* field_type = struct_type->as.struc.field_types[field_index];

        if (!type_assignable(field_type, value_type)) {
            check_error(checker, field->line, field->column,
                        "Cannot initialize field '%s' of type '%s' with '%s'", field_name,
                        type_name(field_type), type_name(value_type));
            had_error = 1;
        }
    }

    for (int i = 0; i < field_count; i++) {
        if (!seen[i]) {
            check_error(checker, init->line, init->column, "Missing initializer for field '%s'",
                        struct_type->as.struc.field_names[i]);
            had_error = 1;
        }
    }

    free(seen);
    return had_error ? type_error : struct_type;
}
