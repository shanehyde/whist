// vec.h - Simple dynamic array growth utility
//
// This header provides a macro to handle the common pattern of growing
// a dynamic array when capacity is exceeded.

#ifndef VEC_H
#define VEC_H

#include "alloc.h"

// Grow array capacity if needed (doubles capacity, starting at initial_cap)
// All parameters must be lvalues. Uses xrealloc for checked allocation.
//
// Usage:
//   VEC_GROW(list->nodes, list->count, list->capacity);
//   list->nodes[list->count++] = item;
//
// Or with custom initial capacity:
//   VEC_GROW_N(array, count, capacity, 64);
//
#define VEC_GROW(array, count, capacity) VEC_GROW_N(array, count, capacity, 8)

#define VEC_GROW_N(array, count, capacity, initial_cap)                                            \
    do {                                                                                           \
        if ((count) >= (capacity)) {                                                               \
            int _new_cap = (capacity) == 0 ? (initial_cap) : (capacity) * 2;                       \
            (array)      = xrealloc((array), (size_t)_new_cap * sizeof(*(array)));                 \
            (capacity)   = _new_cap;                                                               \
        }                                                                                          \
    } while (0)

#endif // VEC_H
