# Whist Bootstrap Compiler (w0)

A bootstrap compiler for the Whist programming language, written in C. Generates C code that can be compiled with any C compiler.

## Build & Run

```bash
make              # Build bin/w0
make test         # Run test suite
make format       # Format source files
```

### Usage

```bash
bin/w0 <file.w>              # Compile to C (stdout)
bin/w0 -o out.c <file.w>     # Compile to C file
bin/w0 --check <file.w>      # Type check only
bin/w0 --ast <file.w>        # Print AST
```

Compile and run a program:

```bash
bin/w0 program.w | cc -x c -o program - && ./program
```

## Language Features

### Types

**Primitive types:**
- `void`, `bool`
- Signed integers: `i8`, `i16`, `i32`, `i64`
- Unsigned integers: `u8`, `u16`, `u32`, `u64`
- Floating point: `f32`, `f64`
- `char`, `string`

**Compound types:**
- Pointers: `*T`
- Arrays: `[n]T` (fixed size)
- Tuples: `(T1, T2, ...)` (heterogeneous, 2+ elements)
- Structs and enums
- Generic structs: `Box<T>`, `Pair<K, V>`
- Spans: `Span<T>` (immutable view into arrays)

### Variables

```whist
var x = 42;              // Type inferred
var y: i64 = 100;        // Explicit type
const PI = 3.14159;      // Immutable constant
var (a, b) = (1, 2);     // Tuple destructuring
```

### Functions

```whist
func add(a: i64, b: i64): i64 {
    return a + b;
}

func greet(): void {
    std.print("Hello!\n");
}
```

### Structs & Methods

```whist
struct Point { x: i64, y: i64 }

func (Point) move(dx: i64, dy: i64): void {
    self->x += dx;
    self->y += dy;
}

func (const Point) distance(): f64 {
    return sqrt(self->x * self->x + self->y * self->y);
}
```

### Generic Structs

```whist
struct Box<T> { value: T }

struct Pair<K, V> { key: K, value: V }

func (Box<T>) get(): T {
    return self->value;
}

func main(): i32 {
    var b: Box<i64> = { value: 42 };
    var p: Pair<string, i64> = { key: "count", value: 10 };
    return 0;
}
```

Generic structs are monomorphized at compile time.

### Enums

```whist
enum Color { Red, Green, Blue }

func main(): i32 {
    var c = Color::Red;
    return 0;
}
```

### Arrays & Spans

```whist
var arr: [5]i64 = [1, 2, 3, 4, 5];    // Fixed-size array
var span: Span<i64> = arr[:];          // Full span
var slice = arr[1:3];                  // Partial span (indices 1, 2)

// Span operations
var len = span.count;                  // Length
var elem = span[0];                    // Bounds-checked access
```

### Tuples

```whist
var t = (42, "hello", true);   // Create tuple
var first = t[0];              // Access by index
var (x, y, z) = t;             // Destructure
```

### Control Flow

```whist
// If-else
if (x > 0) {
    // ...
} else if (x < 0) {
    // ...
} else {
    // ...
}

// While loop
while (condition) {
    if (done) { break; }
    if (skip) { continue; }
}

// C-style for loop
for (var i = 0; i < 10; i += 1) {
    // ...
}

// Foreach range loop
foreach (const i in 0..10) {
    // i goes from 0 to 9
}

foreach (const i in 0..100 by 2) {
    // i goes 0, 2, 4, ..., 98
}
```

### Defer

```whist
func example(): void {
    var handle = open_file();
    defer close_file(handle);   // Runs when function exits
    // ... use handle ...
}
```

### Imports

**Module imports** (from `lib/` directory):
```whist
import std;

func main(): i32 {
    std.print("Hello!\n");       // Module-qualified access
    var x = std.abs_i64(-42);
    return 0;
}
```

**Relative imports** (merge into current namespace):
```whist
import "./helper.w";

func main(): i32 {
    helper_function();   // Direct access
    return 0;
}
```

### Visibility

Declarations are private by default. Use `public` for external linkage:

```whist
public func api_function(): void { }    // Visible externally
func internal_helper(): void { }         // File-local (static in C)
```

### Operators

**Arithmetic:** `+`, `-`, `*`, `/`, `%`

**Comparison:** `==`, `!=`, `<`, `>`, `<=`, `>=`

**Logical:** `&&`, `||`, `!`

**Bitwise:** `&`, `|`, `^`, `~`, `<<`, `>>`

**Assignment:** `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`

**Pointer:** `&` (address-of), `*` (dereference), `->` (member via pointer)

### FFI with C

```whist
extern libc {
    func malloc(size: u64): *void;
    func free(ptr: *void): void;
}

func main(): i32 {
    var ptr = libc.malloc(1024);
    defer libc.free(ptr);
    return 0;
}
```

## License

MIT License - see [LICENSE](LICENSE) for details.
