#include "sem_info.h"

#include <stdlib.h>
#include <string.h>

#include "alloc.h"
#include "vec.h"

typedef struct {
    Node* node;
    int   has_is_ref;
    int   is_ref;
    int   has_is_const_access;
    int   is_const_access;
    int   has_struct_name;
    char* struct_name;
    int   has_module_name;
    char* module_name;
} SemMemberInfo;

typedef struct {
    Node* node;
    int   has_resolved_enum_name;
    char* resolved_enum_name;
    int   has_is_data_enum;
    int   is_data_enum;
} SemEnumValueInfo;

struct SemInfo {
    SemMemberInfo*    members;
    int               member_count;
    int               member_capacity;
    SemEnumValueInfo* enum_values;
    int               enum_value_count;
    int               enum_value_capacity;
};

SemInfo* sem_info_new(void) {
    return xcalloc(1, sizeof(SemInfo));
}

void sem_info_free(SemInfo* info) {
    if (!info) {
        return;
    }

    for (int i = 0; i < info->member_count; i++) {
        free(info->members[i].struct_name);
        free(info->members[i].module_name);
    }
    free(info->members);

    for (int i = 0; i < info->enum_value_count; i++) {
        free(info->enum_values[i].resolved_enum_name);
    }
    free(info->enum_values);

    free(info);
}

static SemMemberInfo* get_member_info(SemInfo* info, Node* member, int create_if_missing) {
    if (!info || !member) {
        return NULL;
    }
    for (int i = 0; i < info->member_count; i++) {
        if (info->members[i].node == member) {
            return &info->members[i];
        }
    }
    if (!create_if_missing) {
        return NULL;
    }

    VEC_GROW(info->members, info->member_count, info->member_capacity);
    SemMemberInfo* out = &info->members[info->member_count++];
    memset(out, 0, sizeof(*out));
    out->node = member;
    return out;
}

static SemEnumValueInfo* get_enum_value_info(SemInfo* info, Node* enum_value, int create_if_missing) {
    if (!info || !enum_value) {
        return NULL;
    }
    for (int i = 0; i < info->enum_value_count; i++) {
        if (info->enum_values[i].node == enum_value) {
            return &info->enum_values[i];
        }
    }
    if (!create_if_missing) {
        return NULL;
    }

    VEC_GROW(info->enum_values, info->enum_value_count, info->enum_value_capacity);
    SemEnumValueInfo* out = &info->enum_values[info->enum_value_count++];
    memset(out, 0, sizeof(*out));
    out->node = enum_value;
    return out;
}

void sem_info_set_member_is_ref(SemInfo* info, Node* member, int is_ref) {
    SemMemberInfo* m = get_member_info(info, member, 1);
    if (!m) {
        return;
    }
    m->has_is_ref = 1;
    m->is_ref     = is_ref != 0;
}

int sem_info_get_member_is_ref(SemInfo* info, Node* member, int fallback) {
    SemMemberInfo* m = get_member_info(info, member, 0);
    if (!m || !m->has_is_ref) {
        return fallback;
    }
    return m->is_ref;
}

void sem_info_set_member_is_const_access(SemInfo* info, Node* member, int is_const_access) {
    SemMemberInfo* m = get_member_info(info, member, 1);
    if (!m) {
        return;
    }
    m->has_is_const_access = 1;
    m->is_const_access     = is_const_access != 0;
}

int sem_info_get_member_is_const_access(SemInfo* info, Node* member, int fallback) {
    SemMemberInfo* m = get_member_info(info, member, 0);
    if (!m || !m->has_is_const_access) {
        return fallback;
    }
    return m->is_const_access;
}

void sem_info_set_member_struct_name(SemInfo* info, Node* member, const char* struct_name) {
    SemMemberInfo* m = get_member_info(info, member, 1);
    if (!m) {
        return;
    }
    m->has_struct_name = 1;
    free(m->struct_name);
    m->struct_name = struct_name ? xstrdup(struct_name) : NULL;
}

const char* sem_info_get_member_struct_name(SemInfo* info, Node* member, const char* fallback) {
    SemMemberInfo* m = get_member_info(info, member, 0);
    if (!m || !m->has_struct_name) {
        return fallback;
    }
    return m->struct_name;
}

void sem_info_set_member_module_name(SemInfo* info, Node* member, const char* module_name) {
    SemMemberInfo* m = get_member_info(info, member, 1);
    if (!m) {
        return;
    }
    m->has_module_name = 1;
    free(m->module_name);
    m->module_name = module_name ? xstrdup(module_name) : NULL;
}

const char* sem_info_get_member_module_name(SemInfo* info, Node* member, const char* fallback) {
    SemMemberInfo* m = get_member_info(info, member, 0);
    if (!m || !m->has_module_name) {
        return fallback;
    }
    return m->module_name;
}

void sem_info_set_enum_value_resolved_name(SemInfo* info, Node* enum_value,
                                           const char* resolved_enum_name) {
    SemEnumValueInfo* e = get_enum_value_info(info, enum_value, 1);
    if (!e) {
        return;
    }
    e->has_resolved_enum_name = 1;
    free(e->resolved_enum_name);
    e->resolved_enum_name = resolved_enum_name ? xstrdup(resolved_enum_name) : NULL;
}

const char* sem_info_get_enum_value_resolved_name(SemInfo* info, Node* enum_value,
                                                   const char* fallback) {
    SemEnumValueInfo* e = get_enum_value_info(info, enum_value, 0);
    if (!e || !e->has_resolved_enum_name) {
        return fallback;
    }
    return e->resolved_enum_name;
}

void sem_info_set_enum_value_is_data_enum(SemInfo* info, Node* enum_value, int is_data_enum) {
    SemEnumValueInfo* e = get_enum_value_info(info, enum_value, 1);
    if (!e) {
        return;
    }
    e->has_is_data_enum = 1;
    e->is_data_enum     = is_data_enum != 0;
}

int sem_info_get_enum_value_is_data_enum(SemInfo* info, Node* enum_value, int fallback) {
    SemEnumValueInfo* e = get_enum_value_info(info, enum_value, 0);
    if (!e || !e->has_is_data_enum) {
        return fallback;
    }
    return e->is_data_enum;
}
