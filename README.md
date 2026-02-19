# Whist

A statically-typed systems programming language that compiles to C.

Whist aims to be a practical language with modern features — generics, a module system, tuples, defer — while staying close to the metal through C code generation. The compiler is currently bootstrapped in C, with a self-hosted compiler planned.

```whist
import std;

func main() -> i32 {
    std.print("Hello, world!\n");
    return 0;
}
```

## Getting Started

```bash
cd w0 && make                    # Build the bootstrap compiler
bin/w0 --lib-path ../lib hello.w | cc -x c -I../lib/include -o hello - && ./hello
```

See [w0/README.md](w0/README.md) for full compiler usage.

## Implemented Features

### Type System
- **Primitive types** — `void`, `bool`, `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `char`, `string`, `voidptr`
- **Pointers** — `*T`, address-of (`&`), dereference (`*`), member access (`->`)
- **Fixed-size arrays** — `[n]T` with array literals
- **Spans** — `Span<T>` immutable views into arrays with bounds-checked access and slicing (`arr[1:3]`)
- **Tuples** — `(T1, T2, ...)` with indexed access and destructuring
- **Structs** — with methods (mutable and const receivers)
- **Enums** — `enum Color { Red, Green, Blue }` with `::` access
- **Generics** — generic structs (`Box<T>`, `Pair<K, V>`) with monomorphization

### Functions & Methods
- Top-level functions with explicit return types
- Methods on structs via `func (Type) method()` syntax
- Const receiver methods via `func (const Type) method()`
- Generic methods on generic structs
- `public` / `private` visibility (private by default)

### Control Flow
- `if` / `else if` / `else`
- `while` loops with `break` / `continue`
- C-style `for` loops
- `foreach` range loops — `foreach (const i in 0..100 by 2)`
- `defer` statements (LIFO cleanup on function exit)

### Module System
- Library imports — `import std;` with qualified access (`std.print(...)`)
- Relative imports — `import "./helper.w";`
- Standard library with `std` (print, abs, max, min) and `fs` (file I/O)

### C Interop
- `extern` blocks for FFI with C libraries
- Varargs support for extern C functions
- Direct C header inclusion for library implementations

### Operators
- Arithmetic: `+` `-` `*` `/` `%`
- Comparison: `==` `!=` `<` `>` `<=` `>=`
- Logical: `&&` `||` `!`
- Bitwise: `&` `|` `^` `~` `<<` `>>`
- Compound assignment: `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`

## Planned Features

### Language
- [ ] **Closures / lambdas** — anonymous functions with variable capture ([design](features/closures.md))
- [ ] **Pattern matching** — `match` expressions with exhaustiveness checking ([design](features/pattern-matching.md))
- [ ] **Result / Option types** — structured error handling with `?` propagation ([design](features/result-option.md))
- [ ] **Traits / interfaces** — shared behavior across types, static and dynamic dispatch ([design](features/traits.md))
- [ ] **String interpolation** — `"Hello {name}!"` ([design](features/string-interpolation.md))
- [ ] **Union types** — `type JsonValue = null | bool | i64 | string` ([design](features/union-types.md))
- [ ] **Type aliases** — `type UserId = i64;` ([design](features/type-aliases.md))
- [ ] **Nullable types** — `?string` with optional chaining ([design](features/nullable-types.md))
- [ ] **Memory management** — reference counting with owner/borrower semantics ([design](features/memory-management.md))
- [ ] **Reflection & comptime** — derive macros, type metadata, compile-time execution ([design](features/reflection.md))

### Compiler
- [ ] **Self-hosted compiler (wc)** — rewrite the compiler in Whist ([design](features/self-hosting.md))
- [ ] **LLVM backend** — native code generation and optimizations ([design](features/llvm-backend.md))
- [ ] **WebAssembly target** — browser and sandboxed execution ([design](features/webassembly.md))
- [ ] **Incremental compilation** — dependency tracking and caching ([design](features/incremental-compilation.md))
- [ ] **Better error messages** — suggested fixes, colored output ([design](features/error-messages.md))

### Tooling
- [ ] **LSP server** — IDE integration with go-to-definition, completions, diagnostics ([design](features/lsp-server.md))
- [ ] **Package manager** — dependency management and registry ([design](features/package-manager.md))
- [ ] **REPL** — interactive evaluation and prototyping ([design](features/repl.md))
- [ ] **Debugger support** — source maps, breakpoints, stack traces ([design](features/debugger.md))

### Standard Library
- [ ] **Collections** — `Vec<T>`, `HashMap<K, V>`, `HashSet<T>`, `Queue<T>`, `Stack<T>` ([design](features/stdlib-collections.md))
- [ ] **Strings** — split, join, trim, search, Unicode support ([design](features/stdlib-strings.md))
- [ ] **Networking** — TCP/UDP sockets, HTTP client ([design](features/stdlib-networking.md))
- [ ] **Math** — trigonometry, random numbers, big integers ([design](features/stdlib-math.md))
- [ ] **Extended I/O** — directory operations, path manipulation, streaming ([design](features/stdlib-io.md))

## Progress

Whist is under active development — the compiler went from initial bootstrap to a language with generics, traits, reference-counted memory management, and a standard library in under two weeks. See the [weekly changelog](PROGRESS.md) for a detailed history of every change.

## Project Structure

```
whist/
├── w0/          Bootstrap compiler (C)
├── wc/          Self-hosted compiler (planned)
├── lib/         Standard library
├── features/       Design documents for future features
└── grammar.md   BNF grammar specification
```

## License

MIT License — see [LICENSE](LICENSE) for details.
