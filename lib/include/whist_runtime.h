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
#include <ctype.h>

/* --- argc/argv globals (defined in whist_runtime.c) --- */
extern int    __w0_argc;
extern char** __w0_argv;

/* --- RC core --- */
typedef struct {
    size_t refcount;
    void (*cleanup)(void*);
} __RcHeader;

/* --- Closure (fat pointer for function values) --- */
typedef struct {
    void* fn;
    void* env;
} __Closure;

#ifdef WHIST_RC_DEBUG

static inline void* __rc_alloc_impl(size_t size, void (*cleanup)(void*),
                                     const char* file, int line) {
    __RcHeader* h = (__RcHeader*)malloc(sizeof(__RcHeader) + size);
    if (!h) {
        fprintf(stderr, "Panic: out of memory\n");
        exit(1);
    }
    h->refcount = 1;
    h->cleanup  = cleanup;
    void* ptr   = (void*)(h + 1);
    fprintf(stderr, "RC_ALLOC: %p (size=%zu, rc=1) at %s:%d\n", ptr, size, file, line);
    return ptr;
}

static inline void __rc_inc_impl(void* ptr, const char* file, int line) {
    if (!ptr)
        return;
    __RcHeader* h = ((__RcHeader*)ptr - 1);
    if (h->refcount == SIZE_MAX)
        return; /* immortal string literal */
    h->refcount++;
    fprintf(stderr, "RC_INC: %p (rc=%zu) at %s:%d\n", ptr, h->refcount, file, line);
}

static inline void __rc_dec_impl(void* ptr, const char* file, int line) {
    if (!ptr)
        return;
    __RcHeader* h = (__RcHeader*)ptr - 1;
    if (h->refcount == SIZE_MAX)
        return; /* immortal string literal */
    if (--h->refcount == 0) {
        if (h->cleanup)
            h->cleanup(ptr);
        fprintf(stderr, "RC_FREE: %p at %s:%d\n", ptr, file, line);
        free(h);
    } else {
        fprintf(stderr, "RC_DEC: %p (rc=%zu) at %s:%d\n", ptr, h->refcount, file, line);
    }
}

#define __rc_alloc(size, cleanup) __rc_alloc_impl(size, cleanup, __FILE__, __LINE__)
#define __rc_inc(ptr) __rc_inc_impl((void*)(ptr), __FILE__, __LINE__)
#define __rc_dec(ptr) __rc_dec_impl((void*)(ptr), __FILE__, __LINE__)

#else /* !WHIST_RC_DEBUG */

static inline void* __rc_alloc(size_t size, void (*cleanup)(void*)) {
    __RcHeader* h = (__RcHeader*)malloc(sizeof(__RcHeader) + size);
    if (!h) {
        fprintf(stderr, "Panic: out of memory\n");
        exit(1);
    }
    h->refcount = 1;
    h->cleanup  = cleanup;
    return (void*)(h + 1);
}

static inline void __rc_inc(void* ptr) {
    if (!ptr)
        return;
    __RcHeader* h = (__RcHeader*)ptr - 1;
    if (h->refcount == SIZE_MAX)
        return; /* immortal string literal */
    h->refcount++;
}

static inline void __rc_dec(void* ptr) {
    if (!ptr)
        return;
    __RcHeader* h = (__RcHeader*)ptr - 1;
    if (h->refcount == SIZE_MAX)
        return; /* immortal string literal */
    if (--h->refcount == 0) {
        if (h->cleanup)
            h->cleanup(ptr);
        free(h);
    }
}

#endif /* WHIST_RC_DEBUG */

/* --- RC string helpers (for C runtime code returning strings to Whist) --- */
static inline const char* __rc_strdup(const char* s) {
    if (!s)
        return NULL;
    size_t len = strlen(s);
    char*  r   = (char*)__rc_alloc(len + 1, NULL);
    memcpy(r, s, len + 1);
    return r;
}

static inline char* __rc_strmalloc(size_t size) {
    return (char*)__rc_alloc(size, NULL);
}

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
    char*  r = (char*)__rc_alloc(la + lb + 1, NULL);
    memcpy(r, a, la);
    memcpy(r + la, b, lb + 1);
    return r;
}

static inline const char* __String_substr(const char* s, int64_t start, int64_t end) {
    int64_t len = end - start;
    if (len < 0)
        len = 0;
    char* r = (char*)__rc_alloc(len + 1, NULL);
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

static inline int64_t __String_index_of(const char* s, const char* pattern) {
    const char* p = strstr(s, pattern);
    return p ? (int64_t)(p - s) : -1;
}

static inline bool __String_ends_with(const char* s, const char* suffix) {
    size_t ls = strlen(s), lsuf = strlen(suffix);
    return ls >= lsuf && strcmp(s + ls - lsuf, suffix) == 0;
}

static inline const char* __String_trim(const char* s) {
    const char* start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    const char* end = s + strlen(s);
    while (end > start && isspace((unsigned char)*(end - 1)))
        end--;
    int64_t len = (int64_t)(end - start);
    char*   r   = (char*)__rc_alloc(len + 1, NULL);
    memcpy(r, start, len);
    r[len] = '\0';
    return r;
}

static inline const char* __String_trim_start(const char* s) {
    const char* start = s;
    while (*start && isspace((unsigned char)*start))
        start++;
    int64_t len = (int64_t)(s + strlen(s) - start);
    char*   r   = (char*)__rc_alloc(len + 1, NULL);
    memcpy(r, start, len);
    r[len] = '\0';
    return r;
}

static inline const char* __String_trim_end(const char* s) {
    size_t      slen = strlen(s);
    const char* end  = s + slen;
    while (end > s && isspace((unsigned char)*(end - 1)))
        end--;
    int64_t len = (int64_t)(end - s);
    char*   r   = (char*)__rc_alloc(len + 1, NULL);
    memcpy(r, s, len);
    r[len] = '\0';
    return r;
}

static inline const char* __String_strip_prefix(const char* s, const char* prefix) {
    size_t plen = strlen(prefix);
    if (strncmp(s, prefix, plen) == 0) {
        return __String_substr(s, (int64_t)plen, (int64_t)strlen(s));
    }
    return __String_substr(s, 0, (int64_t)strlen(s));
}

static inline const char* __String_strip_suffix(const char* s, const char* suffix) {
    size_t slen   = strlen(s);
    size_t suflen = strlen(suffix);
    if (slen >= suflen && strcmp(s + slen - suflen, suffix) == 0) {
        return __String_substr(s, 0, (int64_t)(slen - suflen));
    }
    return __String_substr(s, 0, (int64_t)slen);
}

static inline const char* __String_pad_left(const char* s, int64_t width, char pad) {
    int64_t slen = (int64_t)strlen(s);
    if (slen >= width) {
        char* r = (char*)__rc_alloc(slen + 1, NULL);
        memcpy(r, s, slen + 1);
        return r;
    }
    int64_t pad_count = width - slen;
    char*   r         = (char*)__rc_alloc(width + 1, NULL);
    memset(r, pad, pad_count);
    memcpy(r + pad_count, s, slen + 1);
    return r;
}

static inline const char* __String_pad_right(const char* s, int64_t width, char pad) {
    int64_t slen = (int64_t)strlen(s);
    if (slen >= width) {
        char* r = (char*)__rc_alloc(slen + 1, NULL);
        memcpy(r, s, slen + 1);
        return r;
    }
    char* r = (char*)__rc_alloc(width + 1, NULL);
    memcpy(r, s, slen);
    memset(r + slen, pad, width - slen);
    r[width] = '\0';
    return r;
}

/* --- Vec<string> helpers (needed by __String_split) --- */
typedef struct {
    const char** data;
    int64_t      count;
    int64_t      capacity;
} __Vec_string;

static inline void __Vec_string_cleanup(void* raw) {
    __Vec_string* ptr = (__Vec_string*)raw;
    for (int64_t i = 0; i < ptr->count; i++) {
        __rc_dec((void*)ptr->data[i]);
    }
    free(ptr->data);
}

static inline void __Vec_string_push(__Vec_string* self, const char* value) {
    if (self->count == self->capacity) {
        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;
        self->data      = (const char**)realloc(self->data, new_cap * sizeof(const char*));
        self->capacity  = new_cap;
    }
    __rc_inc((void*)value);
    self->data[self->count++] = value;
}

static inline __Vec_string* __String_split(const char* s, const char* delim) {
    __Vec_string* vec =
        (__Vec_string*)__rc_alloc(sizeof(__Vec_string), __Vec_string_cleanup);
    vec->data     = NULL;
    vec->count    = 0;
    vec->capacity = 0;
    size_t      dlen = strlen(delim);
    const char* p    = s;
    for (;;) {
        const char* found = strstr(p, delim);
        if (!found) {
            const char* part = __String_substr(p, 0, (int64_t)strlen(p));
            __Vec_string_push(vec, part); /* push incs; dec our local ref */
            __rc_dec((void*)part);
            break;
        }
        const char* part = __String_substr(p, 0, (int64_t)(found - p));
        __Vec_string_push(vec, part); /* push incs; dec our local ref */
        __rc_dec((void*)part);
        p = found + dlen;
    }
    return vec;
}

static inline const char* __std_format(const char* fmt, ...) {
    va_list args1, args2;
    va_start(args1, fmt);
    va_copy(args2, args1);
    int n = vsnprintf(NULL, 0, fmt, args1);
    va_end(args1);
    char* buf = (char*)__rc_alloc(n + 1, NULL);
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
        char* e = (char*)__rc_alloc(1, NULL);
        e[0]    = '\0';
        return e;
    }
    char* r = (char*)__rc_alloc(self->count + 1, NULL);
    memcpy(r, self->data, self->count + 1);
    return r;
}

static inline void __StringBuilder_cleanup(void* raw) {
    __StringBuilder* ptr = (__StringBuilder*)raw;
    free(ptr->data);
}

#endif /* WHIST_RUNTIME_H */
