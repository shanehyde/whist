# Traits / Interfaces

Polymorphism beyond generics - define shared behavior across types.

## Phase 1 (Implemented)

Phase 1 covers trait declarations, `impl` blocks, and trait bounds on generic struct type parameters. All dispatch is static (monomorphized). No runtime overhead.

### Trait Declaration

```whist
trait Greetable {
    func greet(): string;
    const func id(): i64;
}
```

Traits define required method signatures. Methods are declared without a receiver or body — just the function signature followed by a semicolon. Use `const func` for methods that require an immutable receiver. Impl blocks must match the const-ness exactly.

### Impl Blocks

```whist
struct Dog {
    name: string,
}

impl Greetable for Dog {
    func greet(): string {
        return self.name;
    }
}
```

An `impl` block provides concrete method implementations for a trait on a specific struct type. Methods inside `impl` blocks do not specify a receiver — it is inferred from the `for Type` clause. Use `const func` for immutable-receiver methods. For generic types, specify the type parameters on the impl header: `impl Drop for Box<T>`. Signatures (parameter types and return type) must match the trait's declaration. All trait methods must be implemented — missing methods are a compile error.

### Trait Bounds on Generic Structs

```whist
trait HasValue {
    func value(): i64;
}

struct Box<T: HasValue> {
    item: T,
}
```

Generic struct type parameters can have trait bounds. When instantiating the generic (`Box<Wrapper>`), the compiler verifies the concrete type implements the required trait. If not, it emits: `Type 'X' does not implement trait 'Y'`.

### Error Detection

The checker validates:
- **Missing methods:** `impl Printable for Empty {}` → error if `to_string()` not provided
- **Return type mismatch:** impl method returns `string` but trait declares `i64`
- **Parameter type/count mismatch:** impl method signature differs from trait
- **Trait bound violations:** generic instantiated with a type that doesn't implement the required trait
- **Unknown trait/type:** referencing nonexistent traits or non-struct types in impl blocks

### Implementation Details

- Traits are registered in the checker's first pass (alongside structs and enums) so they can be forward-referenced
- `TYPE_TRAIT` stores method names and function types (signatures without receiver)
- `TraitImpl` records track which types implement which traits
- Impl methods are processed as regular `NODE_FUNC_DECL` nodes — they get mangled names (`Dog_greet`) and registered on the struct type
- No C code is emitted for trait declarations — traits are purely a type-checking concept
- Trait bounds are stored as `type_param_bounds` on `GenericDef` (parallel to `type_params`)

### Grammar

```bnf
<trait-decl>   ::= 'trait' <identifier> '{' { <trait-method> } '}'
<trait-method>  ::= [ 'const' ] 'func' <identifier> '(' [ <param-list> ] ')' [ ':' <type> ] ';'
<impl-decl>   ::= 'impl' <identifier> 'for' <identifier> [ '<' <type-arg-list> '>' ]
                   '{' { <impl-method> } '}'
<impl-method> ::= [ 'const' ] 'func' <identifier> '(' [ <param-list> ] ')' [ ':' <type> ] '{' <block> '}'
<type-param>  ::= <identifier> [ ':' <identifier> ]
```

## Phase 2 (Future)

Features deferred from Phase 1:

### Default Method Implementations

Methods with bodies in trait declarations, used when impl doesn't override:

```whist
trait Eq {
    func eq(other: Self): bool;
    func ne(other: Self): bool { return !self.eq(other); }
}
```

### Trait Inheritance

Traits that require other traits:

```whist
trait Ord: Eq {
    func cmp(other: Self): Ordering;
}
```

### Multiple Bounds

Constraining a type parameter to implement multiple traits:

```whist
func debug_sort<T: Ord + Debug>(items: Span<T>): void { ... }
```

### Generic Traits

Traits parameterized by types:

```whist
trait Iterator<T> {
    func next(): ?T;
}
```

### Associated Types

Types that are part of a trait definition:

```whist
trait Iterator {
    type Item;
    func next(): ?Self::Item;
}
```

### Trait Bounds on Function Type Parameters

Currently bounds only work on generic struct type params. Extending to function-level generics:

```whist
func sort<T: Ord>(items: Span<T>): void { ... }
```

### Where Clauses

For complex bounds:

```whist
func process<K, V>(map: Map<K, V>): void
where
    K: Eq + Hash,
    V: Clone
{ ... }
```

## Phase 3 (Future)

### Trait Objects (Dynamic Dispatch)

Runtime polymorphism via vtables:

```whist
func log(item: &Printable): void {
    print(item.to_string());
}
```

Would generate:

```c
typedef struct {
    string (*to_string)(void* self);
} Printable_vtable;

typedef struct {
    void* data;
    Printable_vtable* vtable;
} Printable_trait_object;
```

### Orphan Rules

Prevent conflicting implementations — can only implement trait if you own the trait or the type.

### Standard Traits

Core traits the standard library would define: `Eq`, `Ord`, `Clone`, `Debug`, `Default`, `Hash`, `Into<T>`, `From<T>`.

### Operator Overloading

Traits for operator dispatch (`Add`, `Sub`, etc.).

## Open Questions

1. Syntax for trait objects: `&Trait`, `dyn Trait`, `impl Trait`?
2. Allow multiple trait impls for same type?
3. Negative trait bounds (`T: !Copy`)?
4. Marker traits (no methods, just tag types)?
5. Const trait methods?
6. Trait aliases?

## Related Features

- [Closures](closures.md) - Traits for callable types (`Fn`, `FnMut`, `FnOnce`)
- Generics - Trait bounds constrain type parameters
- [Pattern matching](pattern-matching.md) - Matching on trait objects
