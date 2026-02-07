# Vec\<T\> — Dynamic Arrays

Compiler-builtin growable array type, similar to how `Span<T>` is handled.

## Summary

`Vec<T>` is a generic, heap-backed, dynamically-sized array. The compiler knows
about it natively (like `Span<T>`) and emits the struct layout and methods
directly in the generated C. Vecs are RC-managed via `new` and automatically
clean up their backing buffer and elements when freed.

## Syntax

```whist
// Create empty vec
var nums = new Vec<i64>{};

// Create with initial elements
var nums = new Vec<i64>{1, 2, 3};

// Push / pop
nums.push(42);
var last = nums.pop();       // panics if empty

// Indexing (always bounds-checked)
var x = nums[0];
nums[2] = 99;

// Length
var n = nums.count;          // field, consistent with Span<T>

// Clear
nums.clear();

// Span interop
var span: Span<i64> = nums[:];
```

## Type

`Vec<T>` is a new builtin type (`TYPE_VEC`) alongside `TYPE_ARRAY` and
`TYPE_SPAN`. It stores an element type, like Span.

Whist-level view:
```whist
// Conceptual — not user-declared, compiler-provided
struct Vec<T> {
    data: *T,       // backing buffer (plain malloc, not RC)
    count: i64,     // number of elements
    capacity: i64,  // allocated slots
}
```

## C Representation

Each monomorphized `Vec<T>` produces a C struct:

```c
// Vec<i64> →
typedef struct {
    int64_t* data;
    int64_t count;
    int64_t capacity;
} Vec_i64;

// Vec<Point> → (where Point is a struct)
typedef struct {
    Point** data;    // array of pointers (structs are heap-allocated)
    int64_t count;
    int64_t capacity;
} Vec_Point;
```

Since Vecs are created with `new`, the Vec struct itself is RC-managed:

```c
// var nums = new Vec<i64>{};
Vec_i64* nums = (Vec_i64*)__rc_alloc(sizeof(Vec_i64));
nums->data = NULL;
nums->count = 0;
nums->capacity = 0;

// var nums = new Vec<i64>{1, 2, 3};
Vec_i64* nums = (Vec_i64*)__rc_alloc(sizeof(Vec_i64));
nums->data = NULL;
nums->count = 0;
nums->capacity = 0;
__Vec_i64_push(nums, 1);
__Vec_i64_push(nums, 2);
__Vec_i64_push(nums, 3);
```

## V1 API

Minimal surface area. Expand in later iterations.

| Operation | Syntax | Description |
|-----------|--------|-------------|
| Create empty | `new Vec<T>{}` | RC-allocated, empty |
| Create with elements | `new Vec<T>{a, b, c}` | RC-allocated, pushes each |
| Push | `v.push(value)` | Append element, grow if needed |
| Pop | `v.pop()` | Remove and return last element, panic if empty |
| Index read | `v[i]` | Bounds-checked, panic on out-of-range |
| Index write | `v[i] = x` | Bounds-checked, panic on out-of-range |
| Length | `v.count` | Field access (like Span) |
| Clear | `v.clear()` | Remove all elements, cleanup RC if needed |
| Span | `v[:]` / `v[a:b]` | Produce `Span<T>` view |

### Methods not in V1 (future)

- `pop()` returning `Option<T>` instead of panicking
- `insert(index, value)`, `remove(index)`, `swap_remove(index)`
- `reserve(additional)`, `shrink_to_fit()`
- `first()`, `last()` returning `Option<T>`
- `contains(value)`, `sort()`, iteration, functional methods
- `Vec.with_capacity(n)` (needs associated functions)

## Generated C — Method Implementations

The compiler emits these as static functions per monomorphization:

### Push

```c
void __Vec_i64_push(Vec_i64* self, int64_t value) {
    if (self->count == self->capacity) {
        int64_t new_cap = self->capacity == 0 ? 4 : self->capacity * 2;
        self->data = realloc(self->data, new_cap * sizeof(int64_t));
        self->capacity = new_cap;
    }
    self->data[self->count] = value;
    self->count++;
}
```

### Pop

```c
int64_t __Vec_i64_pop(Vec_i64* self) {
    if (self->count == 0) {
        fprintf(stderr, "panic: pop from empty Vec\n");
        exit(1);
    }
    self->count--;
    return self->data[self->count];
}
```

### Index (read)

```c
int64_t __Vec_i64_get(Vec_i64* self, int64_t index) {
    if (index < 0 || index >= self->count) {
        fprintf(stderr, "panic: Vec index %lld out of range (count=%lld)\n",
                index, self->count);
        exit(1);
    }
    return self->data[index];
}
```

### Index (write)

```c
void __Vec_i64_set(Vec_i64* self, int64_t index, int64_t value) {
    if (index < 0 || index >= self->count) {
        fprintf(stderr, "panic: Vec index %lld out of range (count=%lld)\n",
                index, self->count);
        exit(1);
    }
    self->data[index] = value;
}
```

### Clear

```c
void __Vec_i64_clear(Vec_i64* self) {
    // For RC element types: __rc_dec each element first
    self->count = 0;
    // Note: does not free or shrink the backing buffer
}
```

### Drop (compiler-generated `__rc_dec_Vec_T`)

```c
void __rc_dec_Vec_i64(Vec_i64* self) {
    __RcHeader* header = (__RcHeader*)self - 1;
    header->refcount--;
    if (header->refcount == 0) {
        // For RC element types: __rc_dec each element
        free(self->data);
        free(header);
    }
}
```

For `Vec<Point>` where Point is RC-managed:

```c
void __rc_dec_Vec_Point(Vec_Point* self) {
    __RcHeader* header = (__RcHeader*)self - 1;
    header->refcount--;
    if (header->refcount == 0) {
        for (int64_t i = 0; i < self->count; i++) {
            __rc_dec_Point(self->data[i]);  // or __rc_dec depending on type
        }
        free(self->data);
        free(header);
    }
}
```

## RC Integration

- `new Vec<T>{}` → RC-allocated Vec struct (refcount=1)
- Assigning a Vec to another variable → `__rc_inc` (shared reference to same Vec)
- Scope exit → `__rc_dec_Vec_T` (decrements, frees if zero)
- Element cleanup on free: if `T` is RC-managed, dec each element
- Clear also decs RC elements before resetting count

The Vec's backing `data` buffer is **not** RC-managed — it's plain
`malloc`/`realloc`/`free`, owned exclusively by the Vec.

## Span Interop

Slicing a Vec produces a `Span<T>`, reusing the existing slice infrastructure:

```c
// nums[:]
(Span_i64){ .data = nums->data, .count = nums->count }

// nums[1:3]
(Span_i64){ .data = nums->data + 1, .count = 2 }
```

The Span borrows the Vec's buffer — it is only valid while the Vec is alive.
No lifetime enforcement in V1 (same as Span on arrays today).

## Compiler Changes

### types.h / types.c

- Add `TYPE_VEC` to the type enum
- Vec stores element type (like Span): `type->as.vec.elem_type`
- `type_mangle_generic` support: `Vec<i64>` → `Vec_i64`
- `type_equals` / `type_assignable` for Vec types

### lexer

- No changes needed — `Vec` is parsed as an identifier

### parser

- Recognize `Vec<T>` in type position (similar to `Span<T>`)
- Parse `new Vec<T>{...}` — the `{...}` contains expressions (elements),
  not field assignments
- Parse `v.push(expr)`, `v.pop()`, `v.clear()` as method calls
- `v.count` as field access
- `v[i]` / `v[i] = x` already handled by index syntax
- `v[:]` / `v[a:b]` already handled by slice syntax

### checker

- Type-check `new Vec<T>{elems}`: each element must be assignable to `T`
- Type-check method calls: `push` takes `T`, `pop` returns `T`
- `v.count` resolves to `i64`
- Index operations: result type is `T`
- Slice operations: result type is `Span<T>`
- Set `has_rc_fields` if `T` is an RC type

### codegen

- Emit `Vec_T` typedef for each monomorphized Vec type
- Emit method functions (`__Vec_T_push`, `__Vec_T_pop`, etc.)
- Emit `__rc_dec_Vec_T` with element cleanup
- For `new Vec<T>{}`: emit `__rc_alloc` + zero-init
- For `new Vec<T>{a,b,c}`: emit alloc + push calls
- For `v.push(x)`: emit `__Vec_T_push(v, x)`
- For `v[i]`: emit `__Vec_T_get(v, i)`
- For `v[i] = x`: emit `__Vec_T_set(v, i, x)`
- For `v.pop()`: emit `__Vec_T_pop(v)`
- For `v.clear()`: emit `__Vec_T_clear(v)`
- For `v.count`: emit `v->count`
- For `v[:]`: emit Span construction (reuse existing slice codegen)

## Growth Strategy

- Initial capacity: 0 (no allocation until first push)
- Growth factor: 2x
- `realloc` for resizing

## Test Plan

```
test/vec_basic.w          — create, push, index read, count
test/vec_pop.w            — pop, panic on empty
test/vec_init.w           — new Vec<T>{1, 2, 3} initializer
test/vec_bounds.w         — index out-of-range panic (error test)
test/vec_clear.w          — clear, re-push
test/vec_span.w           — slice to Span<T>
test/vec_rc.w             — Vec<StructType> element cleanup
test/vec_index_write.w    — v[i] = x assignment
test/vec_generic.w        — Vec with different element types
```

## Open Questions / Future Work

1. **Passing Vec to functions** — by reference (RC inc/dec) works today.
   Passing by value (copy/clone) is not supported in V1.
2. **Vec of Vec** — `Vec<Vec<i64>>` should work since inner Vec is RC-managed.
   Needs testing.
3. **Equality** — `v1 == v2` for element-wise comparison. Not in V1.
4. **Iteration** — `foreach x in v { ... }`. Needs iterator protocol. Not in V1.
5. **Associated functions** — `Vec.new()`, `Vec.with_capacity()` would be
   cleaner but needs new language feature. Using `new Vec<T>{}` for now.
6. **Capacity control** — no way to pre-allocate in V1.

## Related Features

- [Memory Management](memory-management.md) — RC allocation model
- [Stdlib Collections](stdlib-collections.md) — aspirational full API
- [Traits](traits.md) — Drop trait for cleanup
