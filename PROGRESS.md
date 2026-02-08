# Whist Progress

A weekly changelog of the Whist compiler, tracking development from initial commit to the current state.

---

## Week of Feb 9 – Feb 15, 2026

Self-hosting checklist items land rapidly: string operations, pattern matching with payloads, match exhaustiveness checking, and collection iteration. A refactoring pass continues to reduce function complexity across the compiler.

- **refactor: extract helpers from print_ast and check_binary_expr** ([#101](https://github.com/shanehyde/whist/pull/101)) — further complexity reduction in AST printing and binary expression checking
- **feat: implement foreach over Span\<T\> collections** ([#100](https://github.com/shanehyde/whist/pull/100), [#80](https://github.com/shanehyde/whist/issues/80)) — `foreach item in span { ... }` iterates over Span\<T\> elements
- **feat: implement foreach over Vec\<T\> collections** ([#99](https://github.com/shanehyde/whist/pull/99), [#79](https://github.com/shanehyde/whist/issues/79)) — `foreach item in vec { ... }` iterates over Vec\<T\> elements
- **feat: add match exhaustiveness checking** ([#98](https://github.com/shanehyde/whist/pull/98), [#76](https://github.com/shanehyde/whist/issues/76)) — compiler errors on non-exhaustive match statements, ensuring all enum variants are handled
- **feat: implement match statements with payload binding** ([#97](https://github.com/shanehyde/whist/pull/97), [#75](https://github.com/shanehyde/whist/issues/75)) — match arms can bind enum payloads (`case Some(val) => ...`)
- **feat: implement string operations** ([#96](https://github.com/shanehyde/whist/pull/96), [#67](https://github.com/shanehyde/whist/issues/67)–[#73](https://github.com/shanehyde/whist/issues/73)) — string comparison, concatenation, slicing, and methods (contains, starts_with, ends_with, std.format)
- **feat: implement string.length() method** ([#95](https://github.com/shanehyde/whist/pull/95), [#66](https://github.com/shanehyde/whist/issues/66)) — `str.length()` returns the string length
- **docs: add self-hosting checklist with phased roadmap** ([#65](https://github.com/shanehyde/whist/pull/65)) — comprehensive self-hosting checklist on the GitHub Wiki
- **refactor: extract 36 helper functions from large switch dispatchers** ([#64](https://github.com/shanehyde/whist/pull/64)) — major complexity reduction across checker and codegen; checker split into three files

## Week of Feb 3 – Feb 8, 2026

A big week for memory management and type system maturity. The compiler gained reference-counted heap allocation, a Drop trait for deterministic cleanup, and generic struct support. The type system expanded with tagged unions, generic enums, type aliases, and a builtin Vec\<T\>. Developer tooling improved with a VS Code syntax highlighting extension and a codegen refactor. Several targeted fixes rounded out generic method resolution, enum type inference, and foreach range semantics.

- **fix: accept all integer types in foreach range expressions** ([#62](https://github.com/shanehyde/whist/pull/62)) — foreach ranges now work with any integer type, not just `i32`
- **fix: make foreach range end-exclusive** ([#61](https://github.com/shanehyde/whist/pull/61)) — `foreach i in 0..n` is now end-exclusive (`0` to `n-1`), matching Rust/Python semantics
- **fix: support enum type inference from return type and struct-null comparison** ([#60](https://github.com/shanehyde/whist/pull/60)) — enum variants can be inferred from the function return type; structs can be compared to `null`
- **fix: resolve generic methods not found when type alias triggers instantiation** ([#59](https://github.com/shanehyde/whist/pull/59)) — methods on generic types accessed via type aliases are now resolved correctly
- **fix: codegen crashes and incorrect C for generic struct methods** ([#58](https://github.com/shanehyde/whist/pull/58)) — fixed crashes and wrong C output when calling methods on generic structs
- **feat: add Vec\<T\> as a compiler builtin type** ([#57](https://github.com/shanehyde/whist/pull/57)) — `Vec<T>` is now a builtin with `push`, `pop`, `get`, `set`, and `len` operations
- **feat: add VS Code syntax highlighting extension** ([#56](https://github.com/shanehyde/whist/pull/56)) — TextMate grammar for `.w` files with keyword, type, and literal highlighting
- **feat: add type aliases with generic support** ([#55](https://github.com/shanehyde/whist/pull/55)) — `type Name = ExistingType` and `type Name<T> = Generic<T>` for cleaner APIs
- **refactor: code quality improvements and codegen split** ([#54](https://github.com/shanehyde/whist/pull/54)) — codegen split into multiple files and general code quality cleanup

- **docs: rename plans/ to features/** ([#53](https://github.com/shanehyde/whist/pull/53)) — design documents directory renamed for clarity
- **feat: add generic enums with type parameters** ([#52](https://github.com/shanehyde/whist/pull/52)) — enums can now take type parameters (`enum Option<T> { Some(T), None }`) with monomorphization
- **feat: add enums with data (tagged unions)** ([#51](https://github.com/shanehyde/whist/pull/51)) — enum variants can carry payloads (`enum Shape { Circle(f64), Rect(f64, f64) }`), enabling algebraic data types
- **refactor: remove explicit receiver from impl methods** ([#50](https://github.com/shanehyde/whist/pull/50)) — methods inside `impl` blocks no longer specify a redundant receiver; it's inferred from `impl Trait for Type`. Generic type args move to the impl header (`impl Drop for Box<T>`)
- **refactor: remove stack-based struct initialization** ([#48](https://github.com/shanehyde/whist/pull/48)) — all structs are now consistently heap-allocated via `new`
- **fix: support Drop trait for generic structs and nested RC field cleanup** ([#47](https://github.com/shanehyde/whist/pull/47)) — generic types now properly propagate Drop and clean up nested RC fields
- **feat: add Drop trait and nested RC field cleanup** ([#46](https://github.com/shanehyde/whist/pull/46)) — structs can implement a `drop()` method called automatically when refcount hits zero; nested RC fields are cleaned up transitively
- **fix: rc scope handling and runtime tests** ([#45](https://github.com/shanehyde/whist/pull/45)) — fixed scope-based cleanup and added runtime tests for RC behavior
- **fix: allow --lib-path on run** ([#44](https://github.com/shanehyde/whist/pull/44)) — the `run` subcommand now accepts `--lib-path` for custom library locations
- **feat: add trait declarations, impl blocks, and trait bounds on generics** ([#43](https://github.com/shanehyde/whist/pull/43)) — full trait system with `trait`, `impl`, and `where T: Trait` bounds
- **refactor: fix parser quality issues from code review** ([#42](https://github.com/shanehyde/whist/pull/42)) — parser cleanup and robustness improvements
- **feat: add --rc-debug CLI flag for RC tracking debug output** ([#41](https://github.com/shanehyde/whist/pull/41)) — opt-in debug output showing RC alloc/inc/dec events
- **feat: add reference-counted heap allocation with `new` expression** ([#40](https://github.com/shanehyde/whist/pull/40)) — `new Type { fields }` allocates on the heap with automatic refcount management
- **feat: add voidptr built-in type** ([#39](https://github.com/shanehyde/whist/pull/39)) — added `voidptr` for low-level interop
- **docs: restructure README with language overview and feature roadmap** ([#38](https://github.com/shanehyde/whist/pull/38)) — comprehensive README rewrite
- **feat: add fs module to standard library for file I/O** ([#37](https://github.com/shanehyde/whist/pull/37)) — `fs.open`, `fs.read`, `fs.write`, `fs.close` via stdlib
- **feat: move lib/ to top level and add --lib-path CLI flag** ([#36](https://github.com/shanehyde/whist/pull/36)) — standard library restructured to top-level `lib/` directory
- **feat: add varargs support for extern C functions** ([#35](https://github.com/shanehyde/whist/pull/35)) — `extern` declarations can now use `...` for variadic C functions
- **docs: add comprehensive future plans and design documents** ([#34](https://github.com/shanehyde/whist/pull/34)) — design docs for planned features
- **feat: add Span<T> builtin type and array literals** ([#30](https://github.com/shanehyde/whist/pull/30)) — `Span<T>` for safe, bounded views over contiguous memory; `[1, 2, 3]` array literal syntax
- **refactor: improve w0 compiler maintainability** ([#29](https://github.com/shanehyde/whist/pull/29)) — internal cleanup for long-term maintainability
- **feat: add generic struct types with monomorphization** ([#28](https://github.com/shanehyde/whist/pull/28)) — `struct Pair<T, U> { ... }` with compile-time monomorphization to concrete C types
- **refactor: reduce code duplication with shared utilities** ([#27](https://github.com/shanehyde/whist/pull/27)) — extracted common patterns into shared helpers
- **feat: add nested tuple destructuring support** ([#26](https://github.com/shanehyde/whist/pull/26)) — `var (a, (b, c)) = expr` now works

## Week of Jan 27 – Feb 2, 2026

The project's first full week — starting from a bare bootstrap and rapidly building up the core language: structs, enums, methods, imports, modules, and a real type system. By week's end Whist could compile multi-file programs with qualified imports and extern C interop.

- **refactor: consolidate checker into single file** ([#25](https://github.com/shanehyde/whist/pull/25)) — merged checker source files back into one for easier navigation
- **refactor: consolidate parser into single file** ([#24](https://github.com/shanehyde/whist/pull/24)) — merged parser source files back into one
- **feat: remove increment/decrement operators from language** ([#23](https://github.com/shanehyde/whist/pull/23)) — `++`/`--` removed in favor of `+= 1`/`-= 1`
- **feat: add module qualification requirement for library imports** ([#21](https://github.com/shanehyde/whist/pull/21)) — imports now require `module.symbol` qualification
- **feat: add tuple type support** ([#22](https://github.com/shanehyde/whist/pull/22)) — `(i32, bool)` tuple types with destructuring
- Require module qualification for library imports ([#20](https://github.com/shanehyde/whist/pull/20))
- Add NODE_MODULE to AST for module-based organization ([#19](https://github.com/shanehyde/whist/pull/19))
- Add visibility support for extern function declarations ([#18](https://github.com/shanehyde/whist/pull/18))
- Add module-level visibility for library imports ([#17](https://github.com/shanehyde/whist/pull/17))
- Add relative import support ([#16](https://github.com/shanehyde/whist/pull/16))
- Add import statement support for standard library ([#15](https://github.com/shanehyde/whist/pull/15))
- Add `by` clause for foreach and minor refactors ([#13](https://github.com/shanehyde/whist/pull/13))
- Replace pointers with implicit struct references ([#12](https://github.com/shanehyde/whist/pull/12)) — structs become pointer-based behind the scenes
- Add const modifier for function parameters ([#11](https://github.com/shanehyde/whist/pull/11))
- Add extern C library support ([#10](https://github.com/shanehyde/whist/pull/10)) — call into C libraries from Whist
- Minor tidying ([#9](https://github.com/shanehyde/whist/pull/9))
- Add public visibility modifier for top-level declarations ([#8](https://github.com/shanehyde/whist/pull/8))
- Add 'run' subcommand ([#7](https://github.com/shanehyde/whist/pull/7)) — `whist run file.w` compiles and executes in one step
- Refactor test runner into standalone shell script ([#6](https://github.com/shanehyde/whist/pull/6))
- Restructure: simplify CLAUDE.md and reorganize files ([#5](https://github.com/shanehyde/whist/pull/5))
- Added a `defer` statement to the language
- Split up and restructured source files (multiple commits)
- Fix for immutable methods
- Added grammar definition (BNF)
- Method support — structs can now have methods via `impl`
- Require enums to be qualified (`Enum.Variant`)
- Parser improvements and loop fix
- Foreach statement support
- Struct initialisers
- Added `f32` and `f64` float types
- Renamed int types to explicitly sized `i8`/`i16`/`i32`/`i64`
- Added formatting via `clang-format`
- Enhanced test suite with error-case validation
- Debugging support

## Week of Jan 27, 2026 (Project Start)

- **Initial commit** — bare-bones compiler bootstrap: lexer, parser, checker, and C codegen for a minimal language with functions, variables, if/else, while loops, enums, and structs
