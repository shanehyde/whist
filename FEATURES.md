# Whist Compiler Future Plans

This document outlines potential directions for the Whist compiler and language.
See the [features/](features/) directory for detailed design documents.

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
[Detailed design →](features/closures.md)

Anonymous functions with variable capture:
```whist
var add = |a: i64, b: i64| -> i64 { return a + b; };
var items = filter(list, |x| x > 0);
```

### Traits / Interfaces
[Detailed design →](features/traits.md)

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
[Detailed design →](features/pattern-matching.md)

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
[Detailed design →](features/result-option.md)

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
[Detailed design →](features/string-interpolation.md)

Embedded expressions in strings:
```whist
var name = "world";
print("Hello {name}!");
print("2 + 2 = {2 + 2}");
```

## Type System

### Union Types
[Detailed design →](features/union-types.md)

Values that can be one of several types:
```whist
type JsonValue = null | bool | i64 | f64 | string | JsonArray | JsonObject;
```

### Type Aliases
[Detailed design →](features/type-aliases.md)

Named aliases for complex types:
```whist
type UserId = i64;
type Callback = func(i32, i32): i32;
type StringMap<V> = Map<string, V>;
```

### Nullable Types
[Detailed design →](features/nullable-types.md)

Explicit null safety:
```whist
var name: ?string = null;
if name != null {
    print(name);  // safely unwrapped
}
var len = name?.length() ?? 0;  // optional chaining
```

### Memory Management
[Detailed design →](features/memory-management.md)

Reference counting with owner/borrower semantics for automatic memory management.

### Reflection & Source Generation
[Detailed design →](features/reflection.md)

Compile-time reflection via a phased approach:
1. **Built-in derives** — `@[derive(Debug, Eq, Clone, ...)]` generates implementations from struct/enum definitions
2. **Type descriptor tables** — `@[reflect]` emits static metadata for runtime introspection
3. **Comptime** (wc only) — AST interpreter for user-extensible compile-time code generation

```whist
@[derive(Debug, Eq, Serialize)]
struct User { name: string, age: i64 }

@[reflect]
struct Config { host: string, port: i64 }
var info = @type_info(Config);  // static type metadata
```

## Compiler Infrastructure

### Self-Hosting (wc)
[Detailed design →](features/self-hosting.md)

Rewrite the compiler in Whist itself:
- Validates language expressiveness
- Enables bootstrapping
- Dogfooding finds language gaps
- Milestone for language maturity

### Comptime (wc only)
[Detailed design →](features/comptime.md)

Compile-time execution via an AST interpreter in the self-hosted compiler:
- Comptime functions introspect types via `@type_info`, `@fields` builtins
- `@emit` injects generated source into the compilation
- No separate plugin system — comptime functions are ordinary Whist
- Not in w0 (avoids dual-maintenance of interpreter + codegen in C)

```whist
comptime func gen_eq(comptime T: type): string {
    var info = @type_info(T);
    // ... generate equality function by walking fields ...
}
@emit(gen_eq(Point))
```

### LLVM Backend
[Detailed design →](features/llvm-backend.md)

Native code generation via LLVM:
- Direct machine code output
- Access to LLVM optimizations
- Debug info generation (DWARF)
- Multiple target architectures

### WebAssembly Target
[Detailed design →](features/webassembly.md)

Compile to WASM for:
- Browser execution
- Sandboxed environments
- Portable binaries

### LSP Server
[Detailed design →](features/lsp-server.md)

Language Server Protocol implementation:
- IDE integration (VS Code, etc.)
- Go to definition
- Find references
- Hover information
- Diagnostics
- Auto-completion

### Incremental Compilation
[Detailed design →](features/incremental-compilation.md)

Only recompile changed files:
- Dependency tracking
- Cached intermediate representations
- Faster iteration cycles

## Tooling & Ecosystem

### Package Manager
[Detailed design →](features/package-manager.md)

Dependency management:
- Package registry
- Version resolution
- Lock files
- Build integration

### REPL
[Detailed design →](features/repl.md)

Interactive evaluation:
- Expression evaluation
- State inspection
- Quick prototyping

### Debugger Support
[Detailed design →](features/debugger.md)

Debug info and tooling:
- Source maps
- Breakpoints
- Variable inspection
- Stack traces

### Better Error Messages
[Detailed design →](features/error-messages.md)

Improved diagnostics:
- Suggested fixes
- Similar name hints
- Code snippets with context
- Colored output

## Standard Library

### Collections
[Detailed design →](features/stdlib-collections.md)

- `HashMap<K, V>` - hash-based key-value store
- `HashSet<T>` - unique element collection
- `Vec<T>` - growable array
- `LinkedList<T>` - doubly-linked list
- `Queue<T>` / `Stack<T>` - FIFO/LIFO collections

### File I/O
[Detailed design →](features/stdlib-io.md)

- File reading/writing
- Directory operations
- Path manipulation
- Streaming I/O

### Networking
[Detailed design →](features/stdlib-networking.md)

- TCP/UDP sockets
- HTTP client
- URL parsing

### String Manipulation
[Detailed design →](features/stdlib-strings.md)

- Split, join, trim
- Search and replace
- Unicode support
- Formatting

### Math
[Detailed design →](features/stdlib-math.md)

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
