#ifndef WHIST_CODEGEN_H
#define WHIST_CODEGEN_H

#include <stdio.h>

#include "ast.h"
#include "checker.h"
#include "types.h"

typedef struct {
    FILE* out;
    int   indent;
    int   temp_count; // For generating temporary variables
} CodeGen;

void codegen_init(CodeGen* gen, FILE* out);
void codegen_emit(CodeGen* gen, Node* ast);

#endif
