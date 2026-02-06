# Whist Bootstrap Compiler (w0)

Bootstrap compiler for the Whist programming language, written in C.
Generates C code that can be compiled with any C compiler.

Whist Grammar specification: [grammar.md](../grammar.md)

## Build & Test

```bash
make          # Build bin/w0
make test     # Run test suite
make format   # Format source files (run after modifying .c/.h)
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
bin/w0 program.w | cc -x c -o program -
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
bin/w0 --lib-path ../lib program.w | cc -x c -Ilib/include -o program -
```

**`std.w`** — Core utilities (imported as `import std;`):
- `std.print(s: string)` — print a string
- `std.abs_i64(x: i64)`, `std.max_i64(a, b)`, `std.min_i64(a, b)` — integer math

**`fs.w`** — File I/O (imported as `import fs;`):
- Convenience: `fs.read_file`, `fs.write_file`, `fs.append_file`, `fs.file_exists`, `fs.remove_file`, `fs.rename_file`, `fs.file_size`
- Handle-based: `fs.open`, `fs.close`, `fs.read_line`, `fs.write_string`, `fs.flush`, `fs.seek`, `fs.tell`, `fs.eof`
- Handles are `voidptr` (opaque FILE* pointers); `null` means invalid
- Requires `-Ilib/include` when compiling the generated C (for `fs.h` runtime)

**`lib/include/`** — C header implementations backing extern modules (e.g., `fs.h`).

### Basic Example

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
