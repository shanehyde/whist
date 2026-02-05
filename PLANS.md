# Whist Compiler Future Plans

This document outlines potential directions for the Whist compiler and language.

## Current State

The w0 bootstrap compiler is feature-complete with:
- Full pipeline: lexer → parser → type checker → C code generator
- Generics with monomorphization
- Module system (library and relative imports)
- Methods on structs (mutable and const receivers)
- Tuples with destructuring
- Spans and array literals
- Enums
- Defer statements
- 51 passing test cases

## Proposed Language Features

### Closures / Lambdas
Anonymous functions with variable capture:
```whist
var add = |a: i64, b: i64| -> i64 { return a + b; };
var items = filter(list, |x| x > 0);
```

### Traits / Interfaces
Polymorphism beyond generics:
```whist
trait Printable {
    func to_string(): string;
}

impl Printable for Point {
    func (Point) to_string(): string {
        return "Point(...)";
    }
}
```

### Pattern Matching
`match` expressions with exhaustiveness checking:
```whist
match value {
    0 => print("zero"),
    1..10 => print("small"),
    n if n < 0 => print("negative"),
    _ => print("other"),
}
```

### Result / Option Types
Structured error handling:
```whist
func divide(a: i64, b: i64): Result<i64, string> {
    if b == 0 {
        return Err("division by zero");
    }
    return Ok(a / b);
}

var result = divide(10, 2)?;  // propagate errors
```

### String Interpolation
Embedded expressions in strings:
```whist
var name = "world";
print("Hello {name}!");
print("2 + 2 = {2 + 2}");
```

## Type System

### Union Types
Values that can be one of several types:
```whist
type JsonValue = null | bool | i64 | f64 | string | JsonArray | JsonObject;
```

### Type Aliases
Named aliases for complex types:
```whist
type UserId = i64;
type Callback = func(i32, i32): i32;
type StringMap<V> = Map<string, V>;
```

### Nullable Types
Explicit null safety:
```whist
var name: ?string = null;
if name != null {
    print(name);  // safely unwrapped
}
var len = name?.length() ?? 0;  // optional chaining
```

## Compiler Infrastructure

### Self-Hosting (wc)
Rewrite the compiler in Whist itself:
- Validates language expressiveness
- Enables bootstrapping
- Dogfooding finds language gaps
- Milestone for language maturity

### LLVM Backend
Native code generation via LLVM:
- Direct machine code output
- Access to LLVM optimizations
- Debug info generation (DWARF)
- Multiple target architectures

### WebAssembly Target
Compile to WASM for:
- Browser execution
- Sandboxed environments
- Portable binaries

### LSP Server
Language Server Protocol implementation:
- IDE integration (VS Code, etc.)
- Go to definition
- Find references
- Hover information
- Diagnostics
- Auto-completion

### Incremental Compilation
Only recompile changed files:
- Dependency tracking
- Cached intermediate representations
- Faster iteration cycles

## Tooling & Ecosystem

### Package Manager
Dependency management:
- Package registry
- Version resolution
- Lock files
- Build integration

### REPL
Interactive evaluation:
- Expression evaluation
- State inspection
- Quick prototyping

### Debugger Support
Debug info and tooling:
- Source maps
- Breakpoints
- Variable inspection
- Stack traces

### Better Error Messages
Improved diagnostics:
- Suggested fixes
- Similar name hints
- Code snippets with context
- Colored output

## Standard Library

### Collections
- `HashMap<K, V>` - hash-based key-value store
- `HashSet<T>` - unique element collection
- `Vec<T>` - growable array
- `LinkedList<T>` - doubly-linked list
- `Queue<T>` / `Stack<T>` - FIFO/LIFO collections

### File I/O
- File reading/writing
- Directory operations
- Path manipulation
- Streaming I/O

### Networking
- TCP/UDP sockets
- HTTP client
- URL parsing

### String Manipulation
- Split, join, trim
- Search and replace
- Unicode support
- Formatting

### Math
- Trigonometry
- Random numbers
- Big integers
- Complex numbers

## Priority Considerations

When choosing what to work on, consider:

1. **Self-hosting** - A major milestone that validates the language
2. **Error handling** (Result/Option) - Essential for robust programs
3. **Closures** - Enables functional patterns and callbacks
4. **Better errors** - Improves developer experience immediately
5. **Type aliases** - Low effort, high value

## Notes

This is a living document. Update as priorities shift and features are implemented.
