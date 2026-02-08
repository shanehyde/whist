# Self-Hosting Checklist

Roadmap for making the Whist compiler self-hosting. The w0 bootstrap compiler is ~13K lines of C.
Each item below is a small, shippable feature with a concrete test program.

The strategy: close the gap between what the C compiler uses and what Whist can express,
then port module-by-module (lexer → parser → checker → codegen).

---

## Phase 1: String Operations

The compiler does 125 `strcmp` calls, 50 `sprintf` calls, and constant string slicing.
This is the single biggest gap — strings are the compiler's lifeblood.

- [ ] **1. String length** — `string.length()` method returning `i64`
  - *Test:* assert `"hello".length() == 5`, `"".length() == 0`
  - *Why:* used everywhere to check identifiers, tokens, bounds

- [ ] **2. String equality** — `==` / `!=` operators for `string` type
  - *Test:* `var s = "foo"; assert(s == "foo"); assert(s != "bar");`
  - *Why:* 125 `strcmp` calls in the compiler for token/type comparison

- [ ] **3. String concatenation** — `+` operator for strings (allocates new string)
  - *Test:* `var s = "hello" + " " + "world"; assert(s == "hello world");`
  - *Why:* building mangled type names, error messages, generated code

- [ ] **4. Character indexing** — `string[i]` returning `char`
  - *Test:* `var c = "abc"[1]; assert(c == 'b');`
  - *Why:* lexer scans source character-by-character

- [ ] **5. Substring / slicing** — `string.substr(start, len)` or slice syntax `s[1:4]`
  - *Test:* `assert("hello"[1:4] == "ell");`
  - *Why:* token extraction, identifier parsing

- [ ] **6. String formatting** — `std.format(fmt, ...)` returning `string`
  - *Test:* `var s = std.format("x=%d", 42); assert(s == "x=42");`
  - *Why:* 50 `sprintf` calls for codegen output, error messages, name mangling

- [ ] **7. String ↔ integer conversion** — `std.parse_i64(s)`, `std.to_string(n)`
  - *Test:* `assert(std.parse_i64("42") == 42); assert(std.to_string(42) == "42");`
  - *Why:* parsing integer literals, emitting numeric constants

- [ ] **8. String searching** — `string.contains(s)`, `string.starts_with(s)`, `string.ends_with(s)`
  - *Test:* `assert("hello".starts_with("hel")); assert("hello".contains("ell"));`
  - *Why:* path handling (`./`, `../`), identifier classification

**Milestone:** can write a simple lexer in Whist.

---

## Phase 2: Match Statement

The compiler has 44 `switch` statements, almost all dispatching on `node->type` or `type->kind`.
This is the core control flow pattern for every compiler pass.

- [ ] **9. Match on enums** — `match (expr) { Variant1 => { ... }, Variant2 => { ... } }`
  - *Test:* match on a simple enum, verify correct branch executes
  - *Why:* replaces every `switch (node->type)` in checker/codegen

- [ ] **10. Match with payload binding** — `match (opt) { Some(v) => use(v), None => ... }`
  - *Test:* match on `Option<i64>`, extract and use the payload value
  - *Why:* data enum payloads (AST node fields) need destructuring

- [ ] **11. Match exhaustiveness checking** — error when not all variants covered
  - *Test:* error test: match missing a variant produces compile error
  - *Why:* safety net for AST dispatch — catch missing cases at compile time

**Milestone:** can write an expression evaluator with AST dispatch in Whist.

---

## Phase 3: HashMap Collection

The compiler uses a custom hash table for symbol lookup (scope chains, type registries).
The existing `hash_table.w` test shows it's *possible* with generics, but a built-in is better.

- [ ] **12. HashMap\<K,V\> built-in type** — `new HashMap<K,V>{}` with methods:
  - `map.set(key, value)`, `map.get(key): Option<V>`, `map.has(key): bool`
  - `map.delete(key)`, `map.count`: i64, `map.keys(): Vec<K>`
  - *Test:* insert, lookup, delete, iterate keys
  - *Why:* symbol tables, type caches, import registries

- [ ] **13. String hashing** — strings usable as HashMap keys
  - *Test:* `var m = new HashMap<string, i64>{}; m.set("x", 42);`
  - *Why:* identifier → symbol mapping is the #1 use case

**Milestone:** can implement a symbol table in Whist.

---

## Phase 4: Foreach over Collections

The compiler iterates over dynamic arrays in nearly every function.
Currently `foreach` only supports integer ranges.

- [ ] **14. Foreach over Vec\<T\>** — `foreach (const item in vec) { ... }`
  - *Test:* push items to vec, foreach and sum them, assert correct total
  - *Why:* replaces `for (int i = 0; i < count; i++)` pattern

- [ ] **15. Foreach over Span\<T\>** — `foreach (const item in span) { ... }`
  - *Test:* create span from array, foreach and collect
  - *Why:* same iteration pattern for immutable slices

- [ ] **16. Foreach over HashMap keys** — `foreach (const key in map) { ... }`
  - *Test:* insert entries, iterate, verify all keys visited
  - *Why:* walking symbol tables, collecting types

**Milestone:** can iterate all compiler data structures idiomatically.

---

## Phase 5: Casting & Type Coercions

The compiler casts between integer sizes and uses char ↔ int conversions
constantly in the lexer.

- [ ] **17. Explicit casts** — `expr as Type` syntax
  - *Test:* `var x: i64 = 256; var b: u8 = x as u8; assert(b == 0);`
  - *Why:* u8↔i64, u32↔i64, pointer casts throughout compiler

- [ ] **18. Char ↔ integer** — `c as i32`, `65 as char`
  - *Test:* `assert('A' as i32 == 65); assert(65 as char == 'A');`
  - *Why:* lexer character classification (is_alpha, is_digit, etc.)

**Milestone:** lexer can be fully ported. (~462 lines)

---

## Phase 6: First-Class Functions

Not heavily used in the current compiler, but enables cleaner AST visitors
and comparators for sorting.

- [ ] **19. Function pointer types** — `func(i32, i32): bool` as a type
  - *Test:* pass a named function as argument, call it via the parameter
  - *Why:* callback parameters, generic comparators

- [ ] **20. Anonymous functions (stretch)** — `func(x: i32): i32 { return x + 1; }`
  - *Test:* assign anonymous function to variable, call it
  - *Why:* inline visitors, filter/map callbacks

**Milestone:** can write generic tree walkers.

---

## Phase 7: Error Handling

The compiler uses return codes and error flags. A standard Result type
with match makes this cleaner.

- [ ] **21. Result\<T,E\> in std library** — standard Result enum
  - *Test:* return `Result::Ok(value)` and `Result::Err(msg)`, match on both
  - *Why:* structured error propagation for parse/check phases

- [ ] **22. String error messages** — `Result<Node, string>` pattern
  - *Test:* function that returns error string, caller matches and prints it
  - *Why:* replaces `had_error` flag + fprintf pattern

**Milestone:** parser can return structured errors instead of printing to stderr.

---

## Phase 8: I/O and Process

The compiler reads files, writes to stdout/stderr, parses CLI args,
and invokes the C compiler.

- [ ] **23. stderr output** — `std.eprint(s)` / `std.eprintln(s)`
  - *Test:* program that writes to stderr (redirect and check)
  - *Why:* all diagnostics go to stderr

- [ ] **24. Process arguments** — `std.args(): Vec<string>`
  - *Test:* program that prints its own arguments
  - *Why:* CLI parsing for `-o`, `--check`, `--ast`, etc.

- [ ] **25. Process exit** — `std.exit(code: i32)`
  - *Test:* program that exits with code 42, shell checks `$?`
  - *Why:* fatal error handling (OOM, bad input)

- [ ] **26. Run shell command** — `std.system(cmd: string): i32`
  - *Test:* `std.system("echo hello")` returns 0
  - *Why:* invoking `cc` on generated C code

**Milestone:** `main.c` can be ported — CLI arg parsing, file reading, pipeline orchestration.

---

## Phase 9: Quality of Life

Nice-to-haves that make the ported code more pleasant.

- [ ] **27. Multi-line strings** — triple-quoted `"""..."""` or backtick strings
  - *Test:* multi-line string preserves newlines and indentation
  - *Why:* emitting C code templates in codegen

- [ ] **28. Top-level variables** — module-level `var` with initializer
  - *Test:* top-level `var keywords = ["if", "else", "while"];`
  - *Why:* keyword tables, type singletons, global state

- [ ] **29. Enum integer values** — `enum TokenType { Plus = 43 }`
  - *Test:* enum variant has explicit integer value accessible via cast
  - *Why:* token representation matching ASCII values

---

## Porting Milestones

Once features are in place, port the compiler module by module:

| Order | Module | C Lines | Key Dependencies |
|-------|--------|---------|------------------|
| 1 | **Lexer** | 462 | strings (1-8), casts (17-18) |
| 2 | **AST** | 279+460 | enums, structs, match (9-11) |
| 3 | **Parser** | 2,061 | lexer, AST, Vec, HashMap (12-16) |
| 4 | **Types** | 577+177 | enums, strings, HashMap |
| 5 | **Print AST** | 462 | AST, strings, formatting (6) |
| 6 | **Checker** | 3,167 | everything above, Result (21-22) |
| 7 | **Codegen** | 4,258 | everything above, multi-line strings (27) |
| 8 | **Main** | 399 | I/O (23-26), process args |

**Total: ~12,900 lines of C to port.**

The compiler can be self-hosting once all 8 modules compile with w0 and produce
correct output. At that point, w1 (the Whist-written compiler) replaces w0 as the
primary compiler.
