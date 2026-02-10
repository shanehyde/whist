// util.h - Shared utility functions
//
// Small helpers used across multiple translation units.

#ifndef UTIL_H
#define UTIL_H

#include <stdio.h>
#include <stdlib.h>

#include "alloc.h"

// Read an entire file into a heap-allocated string. Returns NULL on failure.
static inline char* read_file(const char* path) {
    FILE* file = fopen(path, "rb");
    if (!file) {
        return NULL;
    }

    fseek(file, 0, SEEK_END);
    long size = ftell(file);
    fseek(file, 0, SEEK_SET);

    char*  buffer = xmalloc(size + 1);
    size_t read   = fread(buffer, 1, size, file);
    buffer[read]  = '\0';
    fclose(file);
    return buffer;
}

#endif // UTIL_H
