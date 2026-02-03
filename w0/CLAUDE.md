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
```

Compile and run:

```bash
bin/w0 program.w | cc -x c -o program -
```

## Whist Language

Types: `void`, `bool`, `i8`-`i64`, `u8`-`u64`, `f32`, `f64`, `char`, `string`, `*T`, `[n]T`, `struct`, `enum`

Operators: `+ - * / %`, `== != < > <= >=`, `&& || !`, `& | ^ ~ << >>`, `++ --`, `. ->`

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
