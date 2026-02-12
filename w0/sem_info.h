#ifndef WHIST_SEM_INFO_H
#define WHIST_SEM_INFO_H

#include "ast.h"

typedef struct SemInfo SemInfo;

SemInfo* sem_info_new(void);
void     sem_info_free(SemInfo* info);

// NODE_MEMBER metadata
void        sem_info_set_member_is_ref(SemInfo* info, Node* member, int is_ref);
int         sem_info_get_member_is_ref(SemInfo* info, Node* member, int fallback);
void        sem_info_set_member_is_const_access(SemInfo* info, Node* member, int is_const_access);
int         sem_info_get_member_is_const_access(SemInfo* info, Node* member, int fallback);
void        sem_info_set_member_struct_name(SemInfo* info, Node* member, const char* struct_name);
const char* sem_info_get_member_struct_name(SemInfo* info, Node* member, const char* fallback);
void        sem_info_set_member_module_name(SemInfo* info, Node* member, const char* module_name);
const char* sem_info_get_member_module_name(SemInfo* info, Node* member, const char* fallback);

// NODE_ENUM_VALUE metadata
void        sem_info_set_enum_value_resolved_name(SemInfo* info, Node* enum_value,
                                                  const char* resolved_enum_name);
const char* sem_info_get_enum_value_resolved_name(SemInfo* info, Node* enum_value,
                                                  const char* fallback);
void        sem_info_set_enum_value_is_data_enum(SemInfo* info, Node* enum_value, int is_data_enum);
int         sem_info_get_enum_value_is_data_enum(SemInfo* info, Node* enum_value, int fallback);

#endif
