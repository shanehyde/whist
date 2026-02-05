# Memory Management

Automatic memory management for heap-allocated structs using reference counting with owner/borrower semantics.

## Goals

1. **Safety** - No use-after-free, double-free, or memory leaks
2. **Predictable** - Deterministic destruction (no GC pauses)
3. **Ergonomic** - Minimal annotation burden on the programmer
4. **Efficient** - Low overhead, optimize common patterns
5. **C-compatible** - Must compile to reasonable C code

## Current State

Currently, Whist has no automatic memory management:
- Stack allocation for local variables
- No heap allocation syntax
- No ownership tracking
- Programmer must manage memory manually via C FFI

## Core Concept: Owner + Borrower Reference Counting

A hybrid model combining:
- **Reference counting** for automatic deallocation
- **Single owner** for clear responsibility
- **Borrowers** for shared read access with lifetime safety

### The Idea

Every heap allocation has exactly one **owner** reference:
- Owner is responsible for freeing the memory
- Owner's ref count contribution = 1
- When owner goes out of scope, memory is freed

Additional **borrower** references can exist:
- Borrowers can read (and optionally write) the data
- Borrowers increment ref count while alive
- Borrowers must not outlive the owner

```whist
func example(): void {
    var owned point = new Point { x: 10, y: 20 };  // owner, refcount = 1

    var borrowed ref = point;  // borrower, refcount = 2
    print(ref.x);              // use borrower

    // ref goes out of scope, refcount = 1
    // point (owner) goes out of scope, refcount = 0, freed
}
```

## Design Options

### Option A: Compile-Time Borrow Checking (Rust-style)

Statically verify that borrowers don't outlive owners.

```whist
func example(): void {
    var point = new Point { x: 10, y: 20 };
    var ref = &point;          // borrow
    use(ref);
}   // point freed here, ref already out of scope ✓

func bad(): &Point {
    var point = new Point { x: 10, y: 20 };
    return &point;             // ERROR: returning reference to local
}
```

**Pros:**
- Zero runtime overhead for borrow checking
- Errors caught at compile time
- No runtime panics from lifetime violations

**Cons:**
- Complex implementation (borrow checker is hard)
- Steeper learning curve
- Some valid patterns rejected (fighting the borrow checker)
- Significant complexity for a bootstrap compiler

### Option B: Runtime Lifetime Checking

Owner tracks if borrowers exist; panic if owner dies with live borrowers.

```whist
func example(): void {
    var point = new Point { x: 10, y: 20 };  // refcount = 1
    var ref = &point;                         // refcount = 2
    // ref out of scope, refcount = 1
    // point out of scope, refcount = 0, free ✓
}

func bad(): void {
    var ref: &Point;
    {
        var point = new Point { x: 10, y: 20 };
        ref = &point;          // refcount = 2
    }   // point out of scope, refcount = 1, PANIC: owner dying with borrowers!
    use(ref);  // never reached
}
```

**Pros:**
- Simpler implementation than borrow checker
- Catches errors (at runtime)
- More flexible than static checking

**Cons:**
- Runtime overhead (ref count checks)
- Panics instead of compile errors
- Bugs might slip into production

### Option C: Weak Borrowers (Swift-style)

Borrowers become weak references that auto-nil when owner dies.

```whist
func example(): void {
    var point = new Point { x: 10, y: 20 };
    var ref = &point;          // weak reference

    if ref != null {           // check if still valid
        use(ref);
    }
}

func flexible(): void {
    var ref: ?&Point = null;
    {
        var point = new Point { x: 10, y: 20 };
        ref = &point;
    }   // point freed, ref becomes null

    if ref != null {
        use(ref);              // not reached
    }
}
```

**Pros:**
- No panics
- Very flexible
- Familiar from Swift/Objective-C

**Cons:**
- Must always check for null
- Overhead of weak reference tracking
- Silent failures (data disappears)

### Option D: Simple Reference Counting (No Owner Distinction)

All references are equal; last one to leave frees the memory.

```whist
func example(): void {
    var a = new Point { x: 10, y: 20 };  // refcount = 1
    var b = a;                            // refcount = 2
    // b out of scope, refcount = 1
    // a out of scope, refcount = 0, freed
}

func escape(): Point {
    var point = new Point { x: 10, y: 20 };
    return point;              // ownership transferred
}
```

**Pros:**
- Simplest mental model
- Very flexible
- Easy to implement

**Cons:**
- Reference cycles cause leaks
- No clear ownership for reasoning about code
- Can't do in-place mutation optimization

## Handling Reference Cycles

RC's classic problem: cycles never get freed.

```whist
struct Node {
    value: i64,
    next: *Node,    // if this points to self, cycle!
}

var a = new Node { value: 1, next: null };
var b = new Node { value: 2, next: a };
a.next = b;  // cycle: a -> b -> a
// Neither can be freed!
```

### Cycle Solutions

**1. Weak references for back-edges**
```whist
struct Node {
    value: i64,
    next: *Node,          // strong (owned)
    prev: weak *Node,     // weak (borrowed, doesn't prevent freeing)
}
```

**2. Cycle collector**
- Periodically scan for unreachable cycles
- Adds complexity and GC-like behavior

**3. Arenas / regions**
- Allocate related objects together
- Free entire arena at once
- No cycles within arena matter

**4. Programmer responsibility**
- Document that cycles must be broken manually
- Provide `drop()` function to explicitly break cycles

## Proposed Syntax

### Allocation

```whist
// Stack allocation (existing)
var point = Point { x: 10, y: 20 };

// Heap allocation (new)
var point = new Point { x: 10, y: 20 };

// Explicit owned (optional, new is owned by default)
var owned point = new Point { x: 10, y: 20 };
```

### Borrowing

```whist
// Immutable borrow
var ref = &point;
print(ref.x);          // ok
ref.x = 5;             // ERROR: immutable borrow

// Mutable borrow
var ref = &mut point;
ref.x = 5;             // ok
```

### Ownership Transfer

```whist
// Move ownership
var a = new Point { x: 10, y: 20 };
var b = move a;        // a is now invalid
print(a.x);            // ERROR: use after move

// Function takes ownership
func consume(owned p: Point): void {
    // p is freed when function returns
}

// Function borrows
func inspect(p: &Point): void {
    print(p.x);        // just reading
}

// Function returns owned
func create(): owned Point {
    return new Point { x: 0, y: 0 };
}
```

### Explicit Lifetime Annotations (if needed)

```whist
// Reference valid for lifetime 'a
func longest<'a>(x: &'a string, y: &'a string): &'a string {
    if x.len > y.len { return x; }
    return y;
}
```

## Type System Changes

### Pointer Types

```whist
*T          // Raw pointer (unsafe, for C FFI)
owned T     // Owning pointer (or Box<T>)
&T          // Immutable borrow
&mut T      // Mutable borrow
weak T      // Weak reference (optional)
```

### Struct Fields

```whist
struct Container {
    data: owned Data,     // owns this data
    cache: weak Cache,    // doesn't own, may become null
    config: &Config,      // borrows from elsewhere (lifetime tied to container)
}
```

## Implementation Strategy

### Reference Count Storage

**Option 1: Inline header**
```c
typedef struct {
    size_t refcount;
    // actual data follows
} RcHeader;
```

**Option 2: Side table**
- Separate hash table mapping pointers to refcounts
- Less intrusive but slower

**Option 3: Tagged pointers** (if available)
- Use unused bits in pointer for small refcounts
- Fall back to side table for large counts

### Generated C Code

```whist
var point = new Point { x: 10, y: 20 };
var ref = &point;
```

Generates:
```c
Point* point = (Point*)rc_alloc(sizeof(Point));
point->x = 10;
point->y = 20;
// refcount = 1

Point* ref = point;
rc_inc(ref);
// refcount = 2

// end of scope
rc_dec(ref);   // refcount = 1
rc_dec(point); // refcount = 0, free
```

### Optimizations

1. **Elide redundant inc/dec pairs**
   - If borrow doesn't escape, skip refcount ops

2. **Move instead of copy**
   - Transfer ownership instead of inc+dec

3. **Stack allocation when possible**
   - Escape analysis to avoid heap allocation

4. **Inline refcount operations**
   - Avoid function call overhead

## Interaction with Other Features

### Generics

```whist
struct Box<T> {
    value: owned T,
}

impl<T> Box<T> {
    func new(value: T): owned Box<T> {
        return new Box { value: move value };
    }

    func (Box<T>) get(): &T {
        return &self.value;
    }
}
```

### Closures

Closures capturing owned values take ownership:
```whist
var data = new Data { ... };
var closure = move || {
    process(data);  // closure owns data
};
// data is moved into closure, can't use here
```

### Traits

```whist
trait Drop {
    func drop(): void;
}

impl Drop for FileHandle {
    func (FileHandle) drop(): void {
        close_file(self.fd);
    }
}
```

## Open Questions

1. **Owner vs simple RC?**
   - Is the added complexity of owner/borrower worth it?
   - Simple RC is easier but has cycle issues

2. **Static vs runtime borrow checking?**
   - Full borrow checker is complex but safe
   - Runtime checks are simpler but have overhead

3. **Syntax for ownership?**
   - `owned T` vs `Box<T>` vs `~T`
   - `&T` vs `borrowed T`
   - `move x` vs automatic moves

4. **What about interior mutability?**
   - `Cell<T>`, `RefCell<T>` for single-thread
   - Atomic RC for multi-thread

5. **Cycle handling?**
   - Require weak references?
   - Cycle collector?
   - Programmer responsibility?

6. **Default behavior?**
   - Should `new` return owned or RC?
   - Should assignment copy or move?

7. **Integration with C FFI?**
   - How to handle raw pointers from C?
   - Ownership transfer at FFI boundary?

## Recommended Starting Point

For the bootstrap compiler, consider starting simple:

**Phase 1: Simple RC**
- All heap allocations are reference counted
- No owner/borrower distinction
- Programmer handles cycles manually
- Focus: get something working

**Phase 2: Add ownership annotations**
- `owned` and `&` syntax
- Compiler warnings for obvious issues
- No strict enforcement yet

**Phase 3: Borrow checking (optional)**
- Add lifetime analysis
- Compile-time or runtime checking
- Based on experience from phases 1-2

## References

- Rust ownership: https://doc.rust-lang.org/book/ch04-00-understanding-ownership.html
- Swift ARC: https://docs.swift.org/swift-book/documentation/the-swift-programming-language/automaticreferencecounting/
- Lobster: https://aardappel.github.io/lobster/memory_management.html
- Vale: https://vale.dev/guide/regions
- Nim: https://nim-lang.org/docs/destructors.html

## Related Features

- [Result/Option](result-option.md) - Error handling for allocation failures
- [Traits](traits.md) - `Drop` trait for custom destructors
- [Closures](closures.md) - Capture semantics for owned values
