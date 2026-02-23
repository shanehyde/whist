# Whist

A statically-typed application programming language that compiles to C.

Whist aims to be a practical language with modern features — generics, traits, closures, pattern matching, reference counting — while staying close to the metal through C code generation. The compiler is currently bootstrapped in C, with a self-hosted compiler in progress.

```whist
import std;

func main() -> i32 {
    std::println("Hello, world!");
    return 0;
}
```

## Getting Started

```bash
cd w0 && make                    # Build the bootstrap compiler
bin/w0 --lib-path ../lib hello.w | cc -x c -I../lib/include -o hello - ../lib/whist_runtime.c && ./hello
```

See [w0/README.md](w0/README.md) for full compiler usage.

## Implemented Features

### Type System
- **Primitive types** — `void`, `bool`, `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `char`, `string`, `voidptr`
- **Fixed-size arrays** — `[n]T` with array literals
- **Spans** — `Span<T>` immutable views into arrays with bounds-checked access and slicing (`arr[1:3]`)
- **Tuples** — `(T1, T2, ...)` with indexed access and destructuring
- **Structs** — heap-allocated, reference-counted, with methods and `const` fields
- **Enums** — simple (`enum Color { Red, Green, Blue }`), with explicit values, and data enums (tagged unions with payloads)
- **Generics** — generic structs, enums, and functions with monomorphization
- **Type aliases** — `type UserId = i64;`, generic instantiation aliases, partial application
- **Type casting** — `as` for integer/char/enum/pointer conversions
- **Autoboxing** — `Box<T>` wraps primitives with transparent arithmetic and comparison

### Functions & Methods
- Top-level functions with explicit return types
- Methods on structs via `func (Type) method()` syntax via Go like receiver syntax
- Const receiver methods via `func (const Type) method()`
- Generic functions — `func identity<T>(x: T) -> T`
- Generic methods on generic structs — `func (Box<T>) get() -> T`
- Method-level generic type parameters — `func (Vec<T>) map<K>(f: func(T) -> K) -> Vec<K>`
- `const` parameters and return types
- `public` / `private` visibility (private by default)

### Closures & Function Pointers
- **Lambda expressions** — `|x: i64| x * 2`, block body: `|x: i64| -> i64 { return x * 2; }`
- **Closures** capture variables by value from enclosing scope; RC-managed captures are reference-counted
- **Function pointer types** — `func(T1, T2) -> R`, nullable, stored in struct fields
- **Method references** — `var f = Counter.increment;`
- Non-capturing lambdas have zero overhead (null environment)

### Control Flow
- `if` / `else if` / `else`
- `while` loops with `break` / `continue`
- C-style `for` loops
- `foreach` range loops — `foreach (const i in 0..100 by 2)`
- `foreach` over collections — `Vec<T>`, `Span<T>`, `string` (yields `char`)
- `defer` statements (LIFO cleanup on function exit)

### Pattern Matching
- **`match` statement** — on enums, integers, floats, strings, chars, bools
- **`match` expression** — `var area = match (shape) { Circle(r) => r * r, _ => 0.0 };`
- **Data enum destructuring** — `Circle(r) =>`, `Rect(w, h) =>`
- **`is` expression** — `if (opt is Some(v)) { ... }` with payload binding
- **Chained `is`** — `if (opt is Some(v) && v > 0) { ... }`
- **`while` with `is`** — `while (opt is Some(v)) { ... }`
- Wildcard `_` arm, negative literal patterns, qualified variant names

### Traits
- **Trait declarations** — `trait Name { func method(params) -> ReturnType; }`
- **Trait implementations** — `impl TraitName for TypeName { ... }`
- **Generic trait impls** — `impl Drop for Box<T> { ... }`
- **Trait bounds** — `func get_label<T: Printable>(x: T) -> string`
- **`Self` type** in trait signatures — `func clone() -> Self;`
- **Built-in traits** — `Drop` (custom cleanup), `Eq` (enables `==`/`!=`), `Hashable` (for map/set keys)
- **Inherent impl** (no trait) — `impl TypeName { func init(...) }` for constructors

### Error Handling
- **`Option<T>`** — `Some(T)` / `None` with `map`, `and_then`, `value_or`, `expect`, etc.
- **`Result<T, E>`** — `Ok(T)` / `Err(E)` with `map`, `map_err`, `and_then`, `value_or`, etc.
- **Try operator `?`** — unwraps `Ok`/`Some` or propagates `Err`/`None` as early return
- Both auto-imported from the prelude (no `import` needed)

### Memory Management
- **Automatic reference counting** — objects allocated with `new` have inline RC headers
- **Scope-based cleanup** — RC variables automatically decremented at scope exit and before `return`
- **Drop trait** — custom cleanup called when refcount reaches 0
- **Cleanup function pointers** — type-specific cleanup embedded in each allocation
- **RC field tracking** — struct fields of RC types inc/dec'd automatically
- `sameref(a, b)` — reference identity comparison

### Destructuring
- **Tuple destructuring** — `var (a, b) = tuple;`, nested: `var (x, (y, z)) = (1, (2, 3));`
- **Struct destructuring** — `var {x, y} = point;`, partial, with field rename: `var {code, value: val} = info;`
- Wildcard discard — `var (name, _) = pair;`

### Module System
- `import mod;` with qualified access (`mod::func(...)`)
- `include "./helper.w";` — relative include, merges into current namespace
- `use mod::func;` — bring single symbol into scope
- `use mod::{a, b};` — grouped use
- `use mod::*;` — wildcard use

### Strings
- String concatenation with `+`, comparison with `==`/`!=`/`<`/`>`
- **Methods** — `length`, `contains`, `starts_with`, `ends_with`, `index_of`, `split`, `trim`, `trim_start`, `trim_end`, `strip_prefix`, `strip_suffix`, `pad_left`, `pad_right`
- **Indexing** — `s[i]` returns `char`, `s[start:end]` returns substring
- **Interpolated strings** — `$"Hello {name}, age {age + 1}"`
- **Triple-quoted strings** — `"""multi-line with indentation stripping"""`
- **StringBuilder** — `new StringBuilder{}` with `append`, `append_char`, `append_line`, `to_string`, `clear`

### Collections
- **`Vec<T>`** — growable array with `push`, `pop`, `insert`, `remove`, `swap_remove`, `sort`, `contains`, `first`, `last`, `find`, `extend`, `map`, `filter`, `any`, `all`, `each`, `reserve`, `clear`
- **`HashMap<K, V>`** — hash map with `set`, `get`, `has`, `delete`, `keys`
- **`Set<T>`** — hash set with `insert`, `insert_all`, `contains`, `remove`, `values`
- **`Span<T>`** — immutable view with bounds-checked access and `.count`

### C Interop
- `extern` blocks for FFI with C libraries
- Varargs support for extern C functions
- Function aliasing — `func c_name(...) as whist_name;`
- Direct C header inclusion for library implementations

### Testing
- **Inline tests** — `test "name" { ... }` at top level
- **`assert(expr)`** — prints expression and location on failure
- `w0 test file.w` — compiles and runs tests; zero overhead in normal compilation

### Operators
- Arithmetic: `+` `-` `*` `/` `%`
- Comparison: `==` `!=` `<` `>` `<=` `>=`
- Logical: `&&` `||` `!`
- Bitwise: `&` `|` `^` `~` `<<` `>>`
- Compound assignment: `+=` `-=` `*=` `/=` `%=` `&=` `|=` `^=` `<<=` `>>=`

### Literals
- Decimal, hexadecimal (`0xFF`), binary (`0b1010`), octal (`0o755`)
- Float with scientific notation (`1.5e10`)
- Character literals with escape sequences (`\n`, `\t`, `\xNN`, `\NNN`, `\e`)

### Standard Library
- **`std`** — `print`, `println`, `eprint`, `eprintln`, `format`, `args`, `exit`, `system`, `exec`, `parse_i64`, `to_string`, `abs_i64`, `max_i64`, `min_i64`
- **`fs`** — file I/O (`read_file`, `write_file`, `open`, `read_line`), directories (`mkdir`, `rmdir`, `is_dir`, `open_dir`, `read_dir`), paths (`join_path`, `dirname`, `basename`, `extension`, `abs_path`)
- **`collections`** — `HashMap<K, V>`, `Set<T>`
- **`time`** — `time_ms`

## Planned Features

### Language
- [ ] **Nullable types** — `?string` with optional chaining ([design](features/nullable-types.md))
- [ ] **Union types** — `type JsonValue = null | bool | i64 | string` ([design](features/union-types.md))
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
- [ ] **Networking** — TCP/UDP sockets, HTTP client ([design](features/stdlib-networking.md))
- [ ] **Math** — trigonometry, random numbers, big integers ([design](features/stdlib-math.md))
- [ ] **Extended I/O** — streaming, advanced path manipulation ([design](features/stdlib-io.md))

## Progress

Whist is under active development — the compiler went from initial bootstrap to a language with generics, traits, closures, pattern matching, reference-counted memory management, and a standard library in under two weeks. See the [weekly changelog](PROGRESS.md) for a detailed history of every change.

## Project Structure

```
whist/
├── w0/          Bootstrap compiler (C)
├── wc/          Self-hosted compiler (planned)
├── lib/         Standard library and runtime
├── features/    Design documents for future features
└── grammar.md   BNF grammar specification
```

## License

MIT License — see [LICENSE](LICENSE) for details.
