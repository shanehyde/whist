# Traits / Interfaces

Polymorphism beyond generics - define shared behavior across types.

## Proposed Syntax

```whist
trait Printable {
    func to_string(): string;
}

impl Printable for Point {
    func (Point) to_string(): string {
        return "Point({self.x}, {self.y})";
    }
}

// Using traits as bounds
func print_all<T: Printable>(items: Span<T>): void {
    foreach i in 0..items.count {
        print(items[i].to_string());
    }
}
```

## Trait Definition

```whist
trait Iterator<T> {
    // Required method (no body)
    func next(): ?T;

    // Provided method (default implementation)
    func count(): i64 {
        var n = 0;
        while self.next() != null {
            n += 1;
        }
        return n;
    }
}

trait Ord: Eq {  // Trait inheritance
    func cmp(other: Self): Ordering;

    // Default implementations using cmp
    func lt(other: Self): bool { return self.cmp(other) == Ordering::Less; }
    func gt(other: Self): bool { return self.cmp(other) == Ordering::Greater; }
}
```

## Trait Bounds

Constrain generic types to those implementing traits:

```whist
// Single bound
func sort<T: Ord>(items: Span<T>): void { ... }

// Multiple bounds
func debug_sort<T: Ord + Debug>(items: Span<T>): void { ... }

// Where clause for complex bounds
func process<K, V>(map: Map<K, V>): void
where
    K: Eq + Hash,
    V: Clone
{
    // ...
}
```

## Trait Objects (Dynamic Dispatch)

```whist
// Trait object type
var printables: Vec<&Printable> = vec![];
printables.push(&point);
printables.push(&circle);

// As function parameter
func log(item: &Printable): void {
    print(item.to_string());
}
```

## Associated Types

Types that are part of trait definition:

```whist
trait Iterator {
    type Item;

    func next(): ?Self::Item;
}

impl Iterator for Range {
    type Item = i64;

    func (Range) next(): ?i64 {
        // ...
    }
}
```

## Implementation Considerations

### Vtable Generation

For trait objects, generate vtable structs:

```c
// Generated C for trait Printable
typedef struct {
    string (*to_string)(void* self);
} Printable_vtable;

typedef struct {
    void* data;
    Printable_vtable* vtable;
} Printable_trait_object;
```

### Monomorphization vs Dynamic Dispatch

**Static dispatch (generics with bounds):**
- Zero runtime overhead
- Compiler generates specialized code
- Larger binary size

**Dynamic dispatch (trait objects):**
- Runtime overhead (vtable lookup)
- Smaller code size
- Required for heterogeneous collections

### Orphan Rules

Prevent conflicting implementations:
- Can only implement trait if you own the trait OR the type
- Prevents diamond problem scenarios

```whist
// In my_module.w
impl TheirTrait for MyType { }    // OK - own the type
impl MyTrait for TheirType { }    // OK - own the trait
impl TheirTrait for TheirType { } // ERROR - orphan impl
```

## Grammar Changes

```bnf
trait_decl = "trait" IDENTIFIER type_params? (":" trait_bounds)? "{" trait_item* "}"
trait_item = func_signature (";" | block)

impl_decl = "impl" type_params? trait_name "for" type where_clause? "{" impl_item* "}"
impl_item = method_decl

trait_bounds = trait_bound ("+" trait_bound)*
trait_bound = IDENTIFIER type_args?
```

## Standard Traits

Core traits the standard library would define:

```whist
trait Eq {
    func eq(other: Self): bool;
    func ne(other: Self): bool { return !self.eq(other); }
}

trait Ord: Eq {
    func cmp(other: Self): Ordering;
}

trait Clone {
    func clone(): Self;
}

trait Debug {
    func debug_string(): string;
}

trait Default {
    func default(): Self;
}

trait Hash {
    func hash(): u64;
}

trait Into<T> {
    func into(): T;
}

trait From<T> {
    func from(value: T): Self;
}
```

## Open Questions

1. Syntax for trait objects: `&Trait`, `dyn Trait`, `impl Trait`?
2. Allow multiple trait impls for same type?
3. Negative trait bounds (`T: !Copy`)?
4. Marker traits (no methods, just tag types)?
5. Const trait methods?
6. Trait aliases?

## Examples

```whist
// Serialization trait
trait Serialize {
    func to_json(): string;
}

impl Serialize for User {
    func (User) to_json(): string {
        return "{\"name\": \"{self.name}\", \"id\": {self.id}}";
    }
}

// Generic function with trait bound
func save<T: Serialize>(item: T, path: string): Result<void, Error> {
    var json = item.to_json();
    return write_file(path, json);
}

// Operator overloading via traits
trait Add<Rhs = Self> {
    type Output;
    func add(rhs: Rhs): Self::Output;
}

impl Add for Point {
    type Output = Point;
    func (Point) add(rhs: Point): Point {
        return Point { x: self.x + rhs.x, y: self.y + rhs.y };
    }
}
```

## Related Features

- [Closures](closures.md) - Traits for callable types (`Fn`, `FnMut`, `FnOnce`)
- Generics - Trait bounds constrain type parameters
- [Pattern matching](pattern-matching.md) - Matching on trait objects
