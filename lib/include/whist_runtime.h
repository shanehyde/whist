/*
 * whist_runtime.h — Generic C runtime helpers for the Whist compiler
 *
 * Included by every generated .c file via #include <whist_runtime.h>.
 * Contains: RC core (alloc/inc/dec), bounds checks, string helpers,
 * argc/argv globals.
 *
 * Type-specific helpers (__rc_dec_TypeName, __Vec_T_push, etc.) are
 * still emitted per-program by codegen.
 *
 * Define WHIST_RC_DEBUG before including to get fprintf-based RC tracing.
 */

#ifndef WHIST_RUNTIME_H
#define WHIST_RUNTIME_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

/* --- argc/argv globals (defined in whist_runtime.c) --- */
extern int    __w0_argc;
extern char** __w0_argv;

/* --- RC core --- */
typedef struct {
    size_t refcount;
} __RcHeader;

#ifdef WHIST_RC_DEBUG

static inline void* __rc_alloc(size_t size) {
    __RcHeader* h = (__RcHeader*)malloc(sizeof(__RcHeader) + size);
    if (!h) {
        fprintf(stderr, "Panic: out of memory\n");
        exit(1);
    }
    h->refcount = 1;
    void* ptr   = (void*)(h + 1);
    fprintf(stderr, "RC_ALLOC: %p (size=%zu, rc=1)\n", ptr, size);
    return ptr;
}

static inline void __rc_inc(void* ptr) {
    if (!ptr)
        return;
    __RcHeader* h = ((__RcHeader*)ptr - 1);
    h->refcount++;
    fprintf(stderr, "RC_INC: %p (rc=%zu)\n", ptr, h->refcount);
}

static inline void __rc_dec(void* ptr) {
    if (!ptr)
        return;
    __RcHeader* h = (__RcHeader*)ptr - 1;
    if (--h->refcount == 0) {
        fprintf(stderr, "RC_FREE: %p\n", ptr);
        free(h);
    } else {
        fprintf(stderr, "RC_DEC: %p (rc=%zu)\n", ptr, h->refcount);
    }
}

#else /* !WHIST_RC_DEBUG */

static inline void* __rc_alloc(size_t size) {
    __RcHeader* h = (__RcHeader*)malloc(sizeof(__RcHeader) + size);
    if (!h) {
        fprintf(stderr, "Panic: out of memory\n");
        exit(1);
    }
    h->refcount = 1;
    return (void*)(h + 1);
}

static inline void __rc_inc(void* ptr) {
    if (!ptr)
        return;
    ((__RcHeader*)ptr - 1)->refcount++;
}

static inline void __rc_dec(void* ptr) {
    if (!ptr)
        return;
    __RcHeader* h = (__RcHeader*)ptr - 1;
    if (--h->refcount == 0)
        free(h);
}

#endif /* WHIST_RC_DEBUG */

/* --- Bounds checks --- */
static inline void __w0_span_check(uint64_t count, int64_t idx, int line, int col) {
    if (idx < 0 || (uint64_t)idx >= count) {
        fprintf(stderr, "Panic: span index %lld out of bounds (count=%llu) at %d:%d\n",
                (long long)idx, (unsigned long long)count, line, col);
        exit(1);
    }
}

static inline void __w0_vec_check(int64_t count, int64_t idx, int line, int col) {
    if (idx < 0 || idx >= count) {
        fprintf(stderr, "Panic: Vec index %lld out of bounds (count=%lld) at %d:%d\n",
                (long long)idx, (long long)count, line, col);
        exit(1);
    }
}

/* --- Panic --- */
static inline void __whist_panic(const char* msg) {
    fprintf(stderr, "Panic: %s\n", msg);
    exit(1);
}

/* --- String helpers --- */
static inline int64_t __String_length(const char* s) {
    return (int64_t)strlen(s);
}

static inline const char* __String_concat(const char* a, const char* b) {
    size_t la = strlen(a), lb = strlen(b);
    char*  r = (char*)malloc(la + lb + 1);
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

static inline const char* __String_substr(const char* s, int64_t start, int64_t end) {
    int64_t len = end - start;
    if (len < 0)
        len = 0;
    char* r = (char*)malloc(len + 1);
    memcpy(r, s + start, len);
    r[len] = '\0';
    return r;
}

static inline bool __String_contains(const char* s, const char* sub) {
    return strstr(s, sub) != NULL;
}

static inline bool __String_starts_with(const char* s, const char* prefix) {
    return strncmp(s, prefix, strlen(prefix)) == 0;
}

static inline bool __String_ends_with(const char* s, const char* suffix) {
    size_t ls = strlen(s), lsuf = strlen(suffix);
    return ls >= lsuf && strcmp(s + ls - lsuf, suffix) == 0;
}

static inline const char* __std_format(const char* fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);
    int n = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);
    char* buf = (char*)malloc(n + 1);
    vsnprintf(buf, n + 1, fmt, args2);
    va_end(args2);
    return buf;
}

/* --- StringBuilder --- */
typedef struct {
    char*   data;
    int64_t count;
    int64_t capacity;
} __StringBuilder;

static inline void __StringBuilder_append(__StringBuilder* self, const char* s) {
    int64_t len = (int64_t)strlen(s);
    if (self->count + len > self->capacity) {
        int64_t new_cap = self->capacity == 0 ? 64 : self->capacity;
        while (new_cap < self->count + len)
            new_cap *= 2;
        self->data     = (char*)realloc(self->data, new_cap + 1);
        self->capacity = new_cap;
    }
    memcpy(self->data + self->count, s, len);
    self->count += len;
    self->data[self->count] = '\0';
}

static inline void __StringBuilder_append_char(__StringBuilder* self, char c) {
    if (self->count + 1 > self->capacity) {
        int64_t new_cap = self->capacity == 0 ? 64 : self->capacity * 2;
        self->data      = (char*)realloc(self->data, new_cap + 1);
        self->capacity  = new_cap;
    }
    self->data[self->count] = c;
    self->count++;
    self->data[self->count] = '\0';
}

static inline void __StringBuilder_append_line(__StringBuilder* self, const char* s) {
    __StringBuilder_append(self, s);
    __StringBuilder_append_char(self, '\n');
}

static inline int64_t __StringBuilder_len(__StringBuilder* self) {
    return self->count;
}

static inline int64_t __StringBuilder_capacity(__StringBuilder* self) {
    return self->capacity;
}

static inline void __StringBuilder_clear(__StringBuilder* self) {
    self->count = 0;
    if (self->data)
        self->data[0] = '\0';
}

static inline const char* __StringBuilder_to_string(__StringBuilder* self) {
    if (!self->data || self->count == 0) {
        char* e = (char*)malloc(1);
        e[0]    = '\0';
        return e;
    }
    char* r = (char*)malloc(self->count + 1);
    memcpy(r, self->data, self->count + 1);
    return r;
}

static inline void __rc_dec_StringBuilder(__StringBuilder* ptr) {
    if (!ptr)
        return;
    __RcHeader* h = (__RcHeader*)ptr - 1;
    if (--h->refcount == 0) {
        free(ptr->data);
        free(h);
    }
}

#endif /* WHIST_RUNTIME_H */
