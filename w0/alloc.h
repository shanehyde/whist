// alloc.h - Checked allocation wrappers
//
// These wrappers terminate with an error message on allocation failure.
// For a compiler, OOM is rare and typically fatal, so this approach
// simplifies code by eliminating repetitive NULL checks.

#ifndef ALLOC_H
#define ALLOC_H

#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static inline void* xmalloc(size_t size) {
    void* ptr = malloc(size);
    if (!ptr && size > 0) {
        fprintf(stderr, "fatal: out of memory (malloc %zu bytes)\n", size);
        exit(1);
    }
    return ptr;
}

static inline void* xcalloc(size_t count, size_t size) {
    void* ptr = calloc(count, size);
    if (!ptr && count > 0 && size > 0) {
        fprintf(stderr, "fatal: out of memory (calloc %zu * %zu bytes)\n", count, size);
        exit(1);
    }
    return ptr;
}

static inline void* xrealloc(void* ptr, size_t size) {
    void* new_ptr = realloc(ptr, size);
    if (!new_ptr && size > 0) {
        fprintf(stderr, "fatal: out of memory (realloc %zu bytes)\n", size);
        exit(1);
    }
    return new_ptr;
}

static inline char* xstrdup(const char* s) {
    char* copy = strdup(s);
    if (!copy) {
        fprintf(stderr, "fatal: out of memory (strdup)\n");
        exit(1);
    }
    return copy;
}

#endif // ALLOC_H
