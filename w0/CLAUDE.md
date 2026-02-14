# Whist Bootstrap Compiler (w0)

Bootstrap compiler for the Whist programming language, written in C.
Generates C code that can be compiled with any C compiler.

Whist Grammar specification: [grammar.md](../grammar.md)

## LSP

A clang language server (clangd) is configured for this codebase. Use the LSP tool for navigating and understanding C code — it's more reliable than text-based searching for type information and call chains. Key operations:

- `documentSymbol` — list all functions/structs/types in a file
- `hover` — get type signatures and documentation
- `goToDefinition` / `findReferences` — navigate symbol definitions and usages
- `incomingCalls` / `outgoingCalls` — trace call hierarchies

All source files are under `w0/`.

## Build & Test

```bash
make          # Build bin/w0
make test     # Run test suite
make format   # Format source files (run after modifying .c/.h)
make metrics  # Show function sizes/complexity sorted by line count (uses lizard)
```

## Usage

```bash
bin/w0 <file.w>              # Compile to C (stdout)
bin/w0 -o out.c <file.w>     # Compile to C file
bin/w0 --check <file.w>      # Type check only
bin/w0 --ast <file.w>        # Print AST
bin/w0 --lib-path ../lib <file.w>  # Specify library search path
bin/w0 --rc-debug <file.w>        # Emit RC debug tracing in generated code
```

Compile and run:

```bash
bin/w0 --lib-path ../lib program.w | cc -x c -Ilib/include -o program - lib/whist_runtime.c
```

The `--rc-debug` flag adds `fprintf(stderr, ...)` calls to the generated `__rc_alloc`, `__rc_inc`, and `__rc_dec` functions, producing a trace like:

```
RC_ALLOC: 0x600003a04010 (size=16, rc=1)
RC_INC: 0x600003a04010 (rc=2)
RC_DEC: 0x600003a04010 (rc=1)
RC_FREE: 0x600003a04010
```

## Whist Language

Types: `void`, `bool`, `i8`-`i64`, `u8`-`u64`, `f32`, `f64`, `char`, `string`, `voidptr`, `*T`, `[n]T`, `struct`, `enum`

Operators: `+ - * / %`, `== != < > <= >=`, `&& || !`, `& | ^ ~ << >>`, `. ->`

### Imports

**Module imports** (from `lib/` directory) require qualification:
```whist
import std;

func main(): i32 {
    std.print("Hello!\n");        // ✓ Correct: module-qualified
    var x = std.abs_i64(-42);     // ✓ Correct: module-qualified
    // print("Hi!\n");            // ✗ Error: unqualified access
    return 0;
}
```

**Use declarations** selectively bring module symbols into unqualified scope:
```whist
import std;
use std.print;                  // single symbol
use std.{abs_i64, max_i64};    // grouped symbols

func main(): i32 {
    print("Hello!\n");          // ✓ Correct: brought in by use
    var x = abs_i64(-42);       // ✓ Correct: brought in by use
    var y = std.min_i64(1, 2);  // ✓ Correct: qualified still works
    return 0;
}
```

**Relative imports** merge symbols into current namespace:
```whist
import "./helper.w";

func main(): i32 {
    helper_function();  // ✓ Correct: no qualification needed
    return 0;
}
```

### Standard Library (`lib/`)

The Whist standard library lives in the top-level `lib/` directory and is loaded via `--lib-path`:

```bash
bin/w0 --lib-path ../lib program.w | cc -x c -Ilib/include -o program - lib/whist_runtime.c
```

**`std.w`** — Core utilities (imported as `import std;`):
- `std.print(s: string)` — print a string
- `std.abs_i64(x: i64)`, `std.max_i64(a, b)`, `std.min_i64(a, b)` — integer math

**`fs.w`** — File I/O (imported as `import fs;`):
- Convenience: `fs.read_file`, `fs.write_file`, `fs.append_file`, `fs.file_exists`, `fs.remove_file`, `fs.rename_file`, `fs.file_size`
- Handle-based: `fs.open`, `fs.close`, `fs.read_line`, `fs.write_string`, `fs.flush`, `fs.seek`, `fs.tell`, `fs.eof`
- Directory ops: `fs.mkdir`, `fs.mkdir_all`, `fs.rmdir`, `fs.is_dir`, `fs.is_file`, `fs.cwd`, `fs.chdir`
- Directory iteration: `fs.open_dir`, `fs.read_dir`, `fs.close_dir` (handle-based, like file handles)
- Path utilities: `fs.join_path`, `fs.dirname`, `fs.basename`, `fs.extension`, `fs.abs_path`
- Metadata & temp: `fs.modified_time`, `fs.temp_dir`
- Handles are `voidptr` (opaque FILE*/DIR* pointers); `null` means invalid
- Requires `-Ilib/include` when compiling the generated C (for `fs.h` runtime)

**`lib/include/`** — C header implementations backing extern modules (e.g., `fs.h`).

### Basic Example

> See the wiki (`/tmp/whist-wiki`) for full feature docs, grammar spec, and self-hosting checklist.

```whist
struct Point { x: i64, y: i64 }

func add(a: i64, b: i64): i64 {
    return a + b;
}

func (Point) move(dx: i64, dy: i64): void {
    self->x += dx;
    self->y += dy;
}

func main(): i64 {
    var x = 42;
    const PI = 3.14159;
    return 0;
}
```

## Compiler Architecture

The compiler pipeline is: **Lexer** (`lexer.c`) -> **Parser** (`parser.c`) -> **AST** -> **Checker** (`checker.c`, `checker_types.c`, `checker_expr.c`) -> **Codegen** (`codegen.c`, `codegen_types.c`, `codegen_emit.c`, `codegen_expr.c`, `codegen_stmt.c`, `codegen_rc.c`).

### File Responsibilities

| File | Role |
|------|------|
| `lexer.c/h` | Tokenization |
| `parser.c/h` | Recursive descent parsing + import resolution |
| `ast.c/h` | AST node types, creation, cloning, freeing |
| `types.c/h` | Type system, equality, mangling, builtin singletons |
| `checker.c/h` | Init/cleanup, symbol table, statements, declarations |
| `checker_types.c` | Type resolution, generic instantiation |
| `checker_expr.c` | Expression type checking |
| `checker_internal.h` | Cross-file checker function declarations |
| `codegen.c/h` | Codegen orchestration, boilerplate, forward decls, RC runtime |
| `codegen_types.c/h` | Type queries, enum/alias/struct resolution, RC type analysis |
| `codegen_emit.c/h` | Output helpers, type emission, declaration emission |
| `codegen_rc.c` | RC variable tracking (push, cleanup, clear) |
| `codegen_expr.c` | Expression emission |
| `codegen_stmt.c` | Statement emission |
| `codegen_internal.h` | Cross-file codegen function declarations |
| `util.h` | Shared utilities (`read_file`) |
| `alloc.h` | Checked allocation wrappers (`xmalloc`, `xstrdup`, etc.) |
| `vec.h` | Dynamic array growth macro (`VEC_GROW`) |

### Checker Multi-Pass Design

The checker (`checker_check` in `checker.c`) runs four sequential passes over the AST:

1. **Pass 1**: Forward-declare all types (structs, enums, traits) for forward references
2. **Pass 2**: Register generic methods and trait impls on GenericDefs (must happen before instantiation)
3. **Pass 3**: Check all library module declarations (functions, type aliases, impl blocks)
4. **Pass 4**: Check all main module declarations

### Key Conventions

**Module identity**: `checker->current_module` is `NULL` for the main module, set to the module name string for library modules. A symbol's `source_module` follows the same convention -- `NULL` means it belongs to the main module.

**Checker mutates AST nodes**: The checker sets flags on parser-produced AST nodes (marked `// Set by checker` in `ast.h`). These flags are read by codegen. When adding a new checker flag, you must also reset it in `node_clone()` in `ast.c` (cloned bodies are re-checked for each generic instantiation).

Key checker-set flags that codegen depends on:
- `binary.is_string_op` -- triggers `strcmp`/`__String_concat` instead of C operators
- `member.struct_name` / `member.module_name` -- drives method call emission
- `member.is_ref` -- controls `->` vs `.` access
- `var_decl.is_rc` / `var_decl.resolved_type` -- RC tracking and type emission
- `slice.is_string` / `slice.resolved_type` -- string vs array slice emission
- `index.is_tuple_index` -- tuple element access
- `enum_value.is_data_enum` -- tagged union construction
- `call.is_format_call` -- `std.format` builtin handling

**Name mangling**: Generic types become `TypeName_Arg1_Arg2` (e.g., `Box_i64`, `Pair_i64_string`). Methods become `StructName_methodName`. Functions are handled by `type_mangle_generic` in `types.c` and `build_mangled_name_from_generic_node` in `codegen.c`.

**String methods**: The checker uses `struct_name = "__String"` on member access nodes to signal codegen to emit `__String_methodname(self, args...)` calls. The `__String` prefix is a codegen convention, not a real struct.

**RC (Reference Counting)**: Variables created via `new` are tracked in `codegen_rc.c` with scope-based cleanup. Each RC variable has a decrement function -- either generic `__rc_dec` or type-specific `__rc_dec_TypeName` for types with Drop impls or nested RC fields. See the architectural comment at the top of `codegen_emit.c`.

**`codegen_init` parameters**: The codegen receives checker data via a `CodeGenChecker` struct (generic instances, span/vec instances, trait impls) passed by value. The checker must outlive the codegen (borrowed pointers). The `CodeGen` struct uses inline sub-structures: `out` (output), `defer`, `rc`, `generics`, `checker`, `enums`, `aliases`.

### Test Conventions

- Files in `test/valid/` -- compiled with `--check`, expected to pass (exit 0)
- Files in `test/errors/` -- compiled with `--check`, must contain `// Expected error:` comments matching stderr
- Files in `test/rc_runtime/` -- compiled to C, then compiled and run; stdout checked against `// Expected:` comments

### Pitfalls & Gotchas

These are things that are easy to get wrong when modifying the compiler:

**Adding a new checker flag to an AST node**: You must also reset it in `node_clone()` in `ast.c`. Cloned bodies are re-checked for each generic instantiation, so stale flags from a previous instantiation will cause incorrect codegen.

**`check_func_decl` bypasses block scope push** (`checker.c`): The function body is a `NODE_BLOCK`, but we iterate its statements directly instead of calling `check_statement` on the block node, because we already pushed a scope for params. Do NOT simplify this to `check_statement(checker, body)` — it would create a double-scope bug.

**`type_name()` ring buffer has only 4 slots** (`types.c`): Each call rotates through 4 static buffers. Nested type names in a single format string (e.g., `type_name(a), type_name(b)` in one `printf`) can alias if the types are deeply nested generics. Keep format strings to at most 2 `type_name()` calls.

**`token_type_name()` vs `token_type_symbol()`** (`lexer.c`): `token_type_name()` returns enum names like `"PLUS"`, `"LT"` — use only for debugging/AST dumps. For user-facing error messages, use `token_type_symbol()` which returns `"+"`, `"<"`, etc.

**Ownership-transferring functions**: `instantiate_generic_enum` and `instantiate_generic_struct` in `checker_types.c` take ownership of their `mangled` and `resolved_args` parameters (freeing them internally). Do not free these at the call site. Adding an early return inside these functions requires freeing both.

**Shared node ownership in `impl_decl`**: Method `receiver_type_args` share node pointers with `impl_decl.type_args`. In `node_free`, the impl_decl case only frees the array pointer, not the nodes (which are freed by the method's `receiver_type_args`). Do not free these independently.

**RC field detection must include `TYPE_VEC`**: When checking if a struct field or enum variant is RC-managed, the condition must include `TYPE_STRUCT`, `TYPE_VEC`, and `TYPE_ENUM` (if `has_rc_fields`). Missing `TYPE_VEC` will cause memory leaks for fields holding Vec values.

**Parser `>>` token splitting** (`parser.c:237-247`): When parsing `Box<Vec<i64>>`, the lexer produces `>>` as a single token. The parser mutates `parser->current` in-place to split it into `>`. This only works because the modified token is consumed immediately.

**Codegen type substitution pattern**: The pattern of iterating `gen->generics.subst` to find a matching type parameter name appears ~15 times across codegen files. When adding new codegen that needs type parameter substitution, follow the existing pattern in `emit_type` (`codegen_emit.c`).
