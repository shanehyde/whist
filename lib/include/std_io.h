/*
 * std_io.h — I/O helpers for the Whist standard library
 *
 * Functions use the std__ prefix (double underscore) to avoid
 * colliding with the module-prefixed wrapper names (std_*) generated
 * by the whist compiler.
 *
 * Usage: compile with -Ilib/include so #include <std_io.h> resolves.
 */

#ifndef WHIST_STD_IO_H
#define WHIST_STD_IO_H

#include <stdio.h>

static inline void std__eprint(const char* s) {
    fprintf(stderr, "%s", s);
}

static inline void std__eprintln(const char* s) {
    fprintf(stderr, "%s\n", s);
}

#endif
