# Closures / Lambdas

Anonymous functions with variable capture.

## Proposed Syntax

```whist
// Full syntax with types and return type
var add = |a: i64, b: i64| -> i64 { return a + b; };

// Inferred return type
var multiply = |a: i64, b: i64| { return a * b; };

// Single expression (implicit return)
var square = |x: i64| x * x;

// Type inference from context
var items = filter(list, |x| x > 0);
```

## Variable Capture

Closures can capture variables from their enclosing scope:

```whist
func make_adder(n: i64): func(i64): i64 {
    return |x| x + n;  // captures 'n'
}

var add5 = make_adder(5);
print(add5(10));  // 15
```

### Capture Modes

Options to consider:

1. **By value (copy)** - Default, safe, simple
2. **By reference** - Allows mutation, lifetime concerns
3. **Move semantics** - Transfer ownership into closure

```whist
var count = 0;

// Capture by reference (mutable)
var increment = |&| { count += 1; };

// Capture by value (immutable copy)
var snapshot = |=| { return count; };

// Move capture
var take = |move| { return vec; };
```

## Function Types

Need syntax for function pointer / closure types:

```whist
// Function type syntax options:
type Predicate = func(i64): bool;
type Callback = |i64, i64| -> i64;
type Handler = fn(Event) -> void;

// In function signatures
func filter(items: Span<i64>, pred: func(i64): bool): Vec<i64> {
    // ...
}
```

## Implementation Considerations

### Closure Representation

A closure is a struct containing:
- Function pointer to the closure body
- Captured variables (the "environment")

```c
// Generated C for: var add5 = make_adder(5);
typedef struct {
    i64 n;  // captured variable
} Closure_add5_env;

i64 Closure_add5_fn(Closure_add5_env* env, i64 x) {
    return x + env->n;
}
```

### Type Erasure vs Monomorphization

**Option A: Monomorphization** (like generics)
- Each closure gets unique type
- No runtime overhead
- Code bloat for many closures

**Option B: Type erasure** (like Go interfaces)
- All closures with same signature share type
- Small runtime overhead (indirect call)
- Works with dynamic dispatch

### Memory Management

Who owns captured variables?
- Stack closures: reference parent stack frame (unsafe if escapes)
- Heap closures: copy/move captures to heap allocation
- Could use escape analysis to choose automatically

## Grammar Changes

```bnf
closure_expr = "|" param_list? "|" ("->" type)? (block | expression)

param_list = param ("," param)*
param = IDENTIFIER (":" type)?
```

## Dependencies

- May want generics for higher-order functions
- Function types need to be first-class
- Consider interaction with methods (method references?)

## Open Questions

1. Should closures be `Copy` if all captures are `Copy`?
2. How to handle mutable captures safely?
3. Syntax for move captures?
4. Should we support `async` closures later?
5. Method references: `point.move` as closure?

## Examples

```whist
// Map over array
var doubled = map(numbers, |x| x * 2);

// Sort with custom comparator
sort(items, |a, b| a.name < b.name);

// Event handlers
button.on_click(|event| {
    print("Clicked at {event.x}, {event.y}");
});

// Iterators / lazy evaluation
var evens = numbers
    .filter(|x| x % 2 == 0)
    .map(|x| x * x)
    .take(10);
```

## Related Features

- [Traits](traits.md) - For abstracting over callable types
- Higher-kinded types - For generic iterator patterns
- Async/await - Async closures
