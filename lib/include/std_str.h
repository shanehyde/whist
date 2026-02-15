/*
 * std_str.h — String conversion helpers for the Whist standard library
 *
 * Static inline functions using whist-compatible types.
 * Functions use the std__ prefix (double underscore) to avoid
 * colliding with the module-prefixed wrapper names (std_*) generated
 * by the whist compiler.
 *
 * Usage: compile with -Ilib/include so #include <std_str.h> resolves.
 */

#ifndef WHIST_STD_STR_H
#define WHIST_STD_STR_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <whist_runtime.h>

static inline int64_t std__parse_i64(const char* s) {
    return (int64_t)strtoll(s, NULL, 10);
}

static inline const char* std__to_string(int64_t n) {
    char buf[32];
    int len = snprintf(buf, sizeof(buf), "%lld", (long long)n);
    char* r = __rc_strmalloc(len + 1);
    snprintf(r, len + 1, "%lld", (long long)n);
    return r;
}

#endif
