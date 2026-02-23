# Whist Progress

A weekly changelog of the Whist compiler, tracking development from initial commit to the current state.

---

## Week of Feb 23 – Mar 1, 2026

Self-hosting accelerates: the `wc` self-hosted compiler gains a working lexer, sibling module imports, and separate compilation. The bootstrap compiler adds `is` expressions as a cleaner replacement for if-let/while-let, and the standard library modules are compiled to `libwhist.a` for proper separate linking. Default initialization for `Option<T>` rounds out the ergonomics story.

- **feat: compile stdlib modules to libwhist.a for separate linking** — stdlib modules now compile into a static library, enabling clean multi-unit builds
- **feat: replace if-let/while-let with `is` expression** ([#319](https://github.com/shanehyde/whist/pull/319)) — unified `is` syntax for pattern matching in conditionals: `if (opt is Some(v)) { ... }`
- **fix: string literal RC collection for if-let/while-let and lexer escape handling** ([#318](https://github.com/shanehyde/whist/pull/318)) — correct RC tracking for string literals in pattern-matching contexts
- **fix: allow generic base name in qualified match arms** ([#317](https://github.com/shanehyde/whist/pull/317)) — `Option::Some(v)` now resolves correctly for generic enum types
- **refactor(wc): idiomatic method syntax for lexer** ([#316](https://github.com/shanehyde/whist/pull/316)) — self-hosted lexer rewritten to use Whist receiver method syntax
- **feat: sibling module imports and separate compilation for wc** ([#315](https://github.com/shanehyde/whist/pull/315)) — wc modules can import siblings; separate `.w` → `.c` → `.o` compilation pipeline
- **feat(wc): implement self-hosted lexer and run_lex_mode** ([#314](https://github.com/shanehyde/whist/pull/314)) — first working component of the self-hosted compiler: a complete lexer written in Whist
- **fix: preserve short-circuit evaluation for RC-managed temps** ([#311](https://github.com/shanehyde/whist/pull/311)) — `&&`/`||` expressions correctly short-circuit without leaking RC temporaries
- **feat: default Option\<T\> to None when not initialized** ([#310](https://github.com/shanehyde/whist/pull/310)) — `var opt: Option<i64>;` defaults to `None` instead of requiring explicit initialization
- **feat: add --verbose per-test output and test name filter to test_runner** ([#309](https://github.com/shanehyde/whist/pull/309)) — `make test VERBOSE=1` shows per-test results; filter by name substring
- **fix: infer generic enum type params from struct field type** ([#308](https://github.com/shanehyde/whist/pull/308)) — generic enum constructors in struct init now infer type parameters from the field declaration
- **fix: emit \_\_rc\_inc for RC-managed values in enum variant construction** ([#306](https://github.com/shanehyde/whist/pull/306)) — wrapping an RC value in an enum variant (`Some(my_struct)`) now correctly increments the refcount
- **feat: port w0 CLI option processing to wc/main.w** ([#302](https://github.com/shanehyde/whist/pull/302)) — self-hosted compiler entry point processes command-line flags
- **fix: Option\<string\> struct field methods and RC cleanup** ([#303](https://github.com/shanehyde/whist/pull/303)) — methods on `Option<string>` fields in structs now resolve and clean up correctly
- **feat: separate compilation and linking for multi-file projects** ([#299](https://github.com/shanehyde/whist/pull/299)) — `w0` can compile individual `.w` files to `.c` and link them together
- **feat: add top-level Makefile and wc scaffolding** ([#298](https://github.com/shanehyde/whist/pull/298)) — project-wide `make` with targets for w0, wc, lib, and tests
- **refactor: simplify main() and codegen/types helpers** ([#300](https://github.com/shanehyde/whist/pull/300), [#291](https://github.com/shanehyde/whist/pull/291)) — extracted helpers from main and simplified type query functions
- **feat: checker error for assigning to captured value types in lambdas** ([#290](https://github.com/shanehyde/whist/pull/290)) — assigning to a by-value capture now produces a clear error message
- **feat: add autoboxing syntax `var ^name = value`** ([#289](https://github.com/shanehyde/whist/pull/289)) — explicit autoboxing hint for wrapping primitives in `Box<T>`
- **feat: add Box\<T\> built-in type with auto-deref** ([#288](https://github.com/shanehyde/whist/pull/288)) — `Box<T>` wraps a primitive with transparent arithmetic, comparison, and compound assignment
- **refactor: move misplaced functions to correct domains** ([#284](https://github.com/shanehyde/whist/pull/284)) — code organization cleanup across checker and codegen
- **feat: replace shell test runner with Whist-based runner** ([#283](https://github.com/shanehyde/whist/pull/283)) — `make test` now uses a test runner written entirely in Whist
- **refactor: simplify expression emission and var declaration checking** ([#282](https://github.com/shanehyde/whist/pull/282)) — reduced complexity in expression codegen and variable declaration checking
- **fix: generic mangling collisions and deduplicate codegen lookup** ([#281](https://github.com/shanehyde/whist/pull/281)) — generic types with similar names no longer collide in generated C
- **fix: RC leaks in struct destructuring and foreach return** ([#280](https://github.com/shanehyde/whist/pull/280)) — struct destructuring and early return from foreach now clean up correctly
- **feat: replace import with include for relative paths** ([#279](https://github.com/shanehyde/whist/pull/279)) — `include "./file.w"` for relative paths; `import` reserved for library modules
- **feat: add Vec/Set/fs convenience methods and simplify test_runner** ([#278](https://github.com/shanehyde/whist/pull/278)) — `Vec.is_empty()`, `Vec.find()`, `Set.insert_all()`, `fs.is_file()`, and other convenience methods

## Week of Feb 16 – Feb 22, 2026

The language reaches a turning point. Lambdas and closures land in quick succession, giving Whist first-class functions with variable capture. Method-level generic type parameters enable `Vec.map<K>` and `Vec.filter`. Pattern matching expands from enums to integers, strings, and general expressions via `is`-based conditionals. A series of syntax refinements — `::` module separator, `->` return type, `include` for relative paths, private struct fields — bring the language closer to its final surface form. The standard library gains a `Set` type, string trimming/padding, escape sequences, and the test runner starts dogfooding itself. Underneath, an owned-temporaries system plugs a category of RC leaks in expressions.

- **feat: change if-let syntax to `is`-based pattern matching** ([#277](https://github.com/shanehyde/whist/pull/277)) — `if (opt is Some(v))` replaces `if let Some(v) = opt`
- **feat: add private struct fields** ([#276](https://github.com/shanehyde/whist/pull/276)) — `private` modifier on struct fields restricts access to the defining module
- **feat: change function return type syntax from `:` to `->`** ([#275](https://github.com/shanehyde/whist/pull/275)) — `func foo() -> i32` replaces `func foo() : i32`
- **feat: improve test_runner dogfooding** ([#271](https://github.com/shanehyde/whist/pull/271)) — test runner uses more Whist idioms
- **feat: add string trim/strip/pad methods** ([#270](https://github.com/shanehyde/whist/pull/270)) — `trim()`, `trim_start()`, `trim_end()`, `strip_prefix()`, `strip_suffix()`, `pad_left()`, `pad_right()`
- **feat: add Set collection type** ([#269](https://github.com/shanehyde/whist/pull/269)) — `Set<T>` hash set with `insert`, `contains`, `remove`, `values`
- **feat: if-let pattern matching + Result/Option methods** ([#268](https://github.com/shanehyde/whist/pull/268)) — `if let` syntax for enum destructuring; `map`, `and_then`, `unwrap_or_else`, `map_err` methods on Option and Result
- **feat: use compound assignment operators and add missing tests** ([#267](https://github.com/shanehyde/whist/pull/267)) — expanded test coverage for `+=`, `-=`, etc.
- **feat: implement hex, octal, and \\e escape sequences** ([#266](https://github.com/shanehyde/whist/pull/266)) — `\xNN`, `\NNN` octal, and `\e` (ESC) in strings and chars
- **feat: improve test runner + fix RC return param bug** ([#260](https://github.com/shanehyde/whist/pull/260)) — returning a borrowed RC parameter now correctly increments; test runner improvements
- **feat: change module separator from `.` to `::`** ([#259](https://github.com/shanehyde/whist/pull/259)) — `std::println(...)` replaces `std.println(...)`
- **fix: Vec method lookup, tuple codegen, RC leak in tuple destructuring** ([#256](https://github.com/shanehyde/whist/pull/256)) — several fixes for Vec methods on aliased types, tuple codegen edge cases, and tuple destructuring RC
- **feat: update VS Code syntax highlighter to match current grammar** ([#255](https://github.com/shanehyde/whist/pull/255)) — TextMate grammar updated for `::`, `->`, new keywords
- **feat: closures (capturing lambdas)** ([#254](https://github.com/shanehyde/whist/pull/254)) — lambdas can now capture variables from enclosing scopes; RC-managed captures are reference-counted; fat pointer representation with environment
- **feat: renaming on struct destructuring** ([#253](https://github.com/shanehyde/whist/pull/253)) — `var {code, value: val} = info;` renames fields during destructuring
- **feat: lambda parameter type inference** ([#251](https://github.com/shanehyde/whist/pull/251)) — lambda parameters infer types from the expected function signature
- **fix: RC leak in chained method calls** ([#250](https://github.com/shanehyde/whist/pull/250)) — intermediate RC values in method chains are now properly cleaned up
- **fix(generics): handle method-level generic receiver patterns** ([#249](https://github.com/shanehyde/whist/pull/249)) — edge cases in generic method dispatch on receivers
- **feat: method-level generic type parameters** ([#247](https://github.com/shanehyde/whist/pull/247)) — `func (Vec<T>) map<K>(f: func(T) -> K) -> Vec<K>` — methods can introduce their own type parameters beyond the receiver's
- **feat: lambda expressions (non-capturing)** ([#245](https://github.com/shanehyde/whist/pull/245)) — `|x: i64| x * 2` syntax, block bodies, empty params, direct calls; compiled as function pointers with null environment
- **feat: owned temporaries cleanup in control flow** ([#244](https://github.com/shanehyde/whist/pull/244)) — RC temporaries in if/while/for conditions cleaned up at end of enclosing scope
- **feat: match on general expression types** ([#243](https://github.com/shanehyde/whist/pull/243)) — match statements now work on integers, floats, strings, chars, and bools — not just enums
- **feat: owned temporaries system for RC leak prevention** ([#242](https://github.com/shanehyde/whist/pull/242)) — new system tracks RC values returned from function calls and ensures cleanup at statement boundaries
- **fix: RC handling for enum/Vec/StringBuilder types** ([#240](https://github.com/shanehyde/whist/pull/240)) — correct inc/dec for enum payloads, Vec elements, and StringBuilder in various contexts
- **refactor(parser,codegen): split parser files and simplify codegen traversal** ([#239](https://github.com/shanehyde/whist/pull/239)) — parser split into multiple files; codegen traversal simplified

## Week of Feb 9 – Feb 15, 2026

An enormous week that transforms Whist from a basic compiled language into a practical one. The standard library expands with Vec operations (insert, remove, sort, contains, first/last), Option/Result methods, StringBuilder, string splitting/ordering, struct destructuring, and a filesystem module. The type system gains function pointer types, Self in traits, generic free functions with trait bounds, Eq for value equality, and struct constructors via `init`. Reference counting matures with cleanup function pointers in the RC header, RC-tracked strings with immortal literals, and fixes for implicit void returns, Vec index writes, and anonymous `new` in call args. A test framework is built directly in Whist, and the test runner is rewritten in the language itself.

- **fix: hoist anonymous `new` in call args to prevent RC leak** ([#236](https://github.com/shanehyde/whist/pull/236)) — `items.push(new Item{value: 10})` no longer leaks; args hoisted to temps and dec'd after the call
- **feat: user-defined methods on Vec\<T\>** ([#234](https://github.com/shanehyde/whist/pull/234)) — user code can define methods on `Vec<T>` via `impl` blocks (map, filter, etc.)
- **fix: emit RC cleanup for implicit void returns and Vec index writes** ([#232](https://github.com/shanehyde/whist/pull/232)) — functions without explicit return now clean up RC vars; Vec index assignment properly dec's old values
- **fix: track Vec/StringBuilder from function calls as RC-managed** ([#231](https://github.com/shanehyde/whist/pull/231)) — Vec and StringBuilder returned from function calls are now tracked for scope cleanup
- **feat: reference-counted strings with immortal literals** ([#230](https://github.com/shanehyde/whist/pull/230)) — strings are RC-managed; string literals use a high refcount to prevent collection
- **feat: struct destructuring `var {field1, field2} = expr;`** ([#229](https://github.com/shanehyde/whist/pull/229)) — pull individual fields from a struct into local variables
- **fix: replace system() with fork/exec for proper signal handling** ([#228](https://github.com/shanehyde/whist/pull/228)) — `std.exec` uses fork/exec for correct exit code propagation
- **feat: Vec\<string\>.sort() support** ([#227](https://github.com/shanehyde/whist/pull/227)) — sorting vectors of strings
- **feat: string.index_of() method** ([#226](https://github.com/shanehyde/whist/pull/226)) — `s.index_of("sub")` returns position or -1
- **feat: string.split() method** ([#225](https://github.com/shanehyde/whist/pull/225)) — `s.split(",")` returns `Vec<string>`
- **feat: string ordering operators** ([#224](https://github.com/shanehyde/whist/pull/224)) — `<`, `>`, `<=`, `>=` for string comparison via strcmp
- **feat: write test runner in Whist** ([#217](https://github.com/shanehyde/whist/pull/217)) — the test runner is now a Whist program that compiles and runs test files
- **feat: add std.exec for capturing command output** ([#216](https://github.com/shanehyde/whist/pull/216)) — `std::exec(cmd)` returns `ExecResult` with exit code, stdout, and stderr
- **feat: multi-line triple-quoted strings** ([#214](https://github.com/shanehyde/whist/pull/214)) — `"""..."""` with automatic indentation stripping
- **feat: add directory, path, and metadata operations to fs module** ([#213](https://github.com/shanehyde/whist/pull/213)) — `mkdir`, `rmdir`, `is_dir`, `open_dir`, `read_dir`, `join_path`, `dirname`, `basename`, `extension`, `abs_path`, `modified_time`, `temp_dir`
- **feat: Vec == and != operators** ([#212](https://github.com/shanehyde/whist/pull/212)) — element-wise equality comparison for vectors
- **feat: struct initializers via `init` in impl blocks** ([#211](https://github.com/shanehyde/whist/pull/211)) — `new Point(1, 2)` constructor syntax backed by `impl Point { func init(...) { ... } }`
- **feat: duck-type trait conformance with signature-only impl methods** ([#208](https://github.com/shanehyde/whist/pull/208)) — impl blocks can declare method signatures without bodies; conformance checked against standalone methods
- **feat: generic free functions with trait bounds and type inference** ([#205](https://github.com/shanehyde/whist/pull/205)) — `func get_label<T: Printable>(x: T)` with type argument inference at call sites
- **feat: embed cleanup function pointer in RC header** ([#204](https://github.com/shanehyde/whist/pull/204)) — each RC allocation stores its type-specific cleanup function, enabling universal `__rc_dec`
- **feat: Vec.pop() returns Option\<T\> instead of panicking** ([#197](https://github.com/shanehyde/whist/pull/197)) — pop on empty vec returns `None` instead of aborting
- **feat: add built-in unit testing with test blocks and assert** ([#196](https://github.com/shanehyde/whist/pull/196)) — `test "name" { assert(expr); }` with `w0 test file.w`
- **feat: add value equality via Eq trait and sameref builtin** ([#191](https://github.com/shanehyde/whist/pull/191)) — structs implementing `Eq` can use `==`/`!=`; `sameref(a, b)` for pointer identity
- **feat: add function pointer types** ([#190](https://github.com/shanehyde/whist/pull/190)) — `func(i64) -> bool` as a first-class type; nullable; storable in struct fields
- **feat(checker): support Self type in traits and impl blocks** ([#188](https://github.com/shanehyde/whist/pull/188)) — `func clone() -> Self;` in trait signatures resolves to the implementing type
- **feat(builtin): add StringBuilder type** ([#187](https://github.com/shanehyde/whist/pull/187)) — mutable string builder with `append`, `append_char`, `append_line`, `to_string`, `clear`
- **feat(std): add eprint and eprintln for stderr output** ([#184](https://github.com/shanehyde/whist/pull/184)) — stderr printing via `std::eprint` and `std::eprintln`
- **feat(checker): enforce top-level const, reject top-level var** ([#183](https://github.com/shanehyde/whist/pull/183)) — top-level variables must be `const`; mutable `var` restricted to function bodies
- **feat(vec): add contains and sort methods** ([#181](https://github.com/shanehyde/whist/pull/181)) — `vec.contains(x)` and `vec.sort()` for orderable types
- **feat(prelude): add value(), expect(), value\_or(), error() on Option and Result** ([#180](https://github.com/shanehyde/whist/pull/180)) — unwrapping methods for Option and Result
- **feat(enums): support generic enum methods and add Option/Result queries** ([#179](https://github.com/shanehyde/whist/pull/179)) — methods on generic enums; `has_value()`, `is_ok()`, `is_err()` queries
- **feat(vec): add first() and last() methods returning Option\<T\>** ([#178](https://github.com/shanehyde/whist/pull/178)) — safe access to vec endpoints
- **feat(vec): add insert, remove, and swap\_remove for Vec** ([#177](https://github.com/shanehyde/whist/pull/177)) — index-based insertion, removal, and O(1) swap removal
- **refactor: reduce emit\_stmt complexity** ([#189](https://github.com/shanehyde/whist/pull/189)) — statement emission broken into smaller helper functions
- **refactor: simplify codegen forward decls and checker call handling** ([#206](https://github.com/shanehyde/whist/pull/206)) — forward declaration generation simplified
- **refactor: convert run tests to test blocks** ([#210](https://github.com/shanehyde/whist/pull/210)) — test suite migrated from `main()`-based tests to `test` blocks
- **chore(tests): reorganize suite and runner into run/errors structure** ([#203](https://github.com/shanehyde/whist/pull/203)) — tests organized into run/ (should succeed) and errors/ (should fail) directories
- **docs(grammar): sync grammar with bootstrap compiler** ([#198](https://github.com/shanehyde/whist/pull/198)) — BNF grammar updated to match all implemented features
- **refactor: extract helpers from print\_ast and check\_binary\_expr** ([#101](https://github.com/shanehyde/whist/pull/101)) — further complexity reduction in AST printing and binary expression checking
- **feat: implement foreach over Span\<T\> collections** ([#100](https://github.com/shanehyde/whist/pull/100), [#80](https://github.com/shanehyde/whist/issues/80)) — `foreach item in span { ... }` iterates over Span\<T\> elements
- **feat: implement foreach over Vec\<T\> collections** ([#99](https://github.com/shanehyde/whist/pull/99), [#79](https://github.com/shanehyde/whist/issues/79)) — `foreach item in vec { ... }` iterates over Vec\<T\> elements
- **feat: add match exhaustiveness checking** ([#98](https://github.com/shanehyde/whist/pull/98), [#76](https://github.com/shanehyde/whist/issues/76)) — compiler errors on non-exhaustive match statements, ensuring all enum variants are handled
- **feat: implement match statements with payload binding** ([#97](https://github.com/shanehyde/whist/pull/97), [#75](https://github.com/shanehyde/whist/issues/75)) — match arms can bind enum payloads (`case Some(val) => ...`)
- **feat: implement string operations** ([#96](https://github.com/shanehyde/whist/pull/96), [#67](https://github.com/shanehyde/whist/issues/67)–[#73](https://github.com/shanehyde/whist/issues/73)) — string comparison, concatenation, slicing, and methods (contains, starts\_with, ends\_with, std.format)
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
