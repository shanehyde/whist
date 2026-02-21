# Whist Language Feature Reference

Comprehensive reference for Whist language features, organized for porting C code to Whist.

## Types

### Primitives

```whist
var a: i8 = 127;            // Signed 8-bit
var b: i16 = 32767;         // Signed 16-bit
var c: i32 = 100;           // Signed 32-bit
var d: i64 = 42;            // Signed 64-bit
var e: u8 = 255;            // Unsigned 8-bit
var f: u16 = 65535;         // Unsigned 16-bit
var g: u32 = 100;           // Unsigned 32-bit
var h: u64 = 100;           // Unsigned 64-bit
var i: f32 = 2.718;         // 32-bit float
var j: f64 = 3.14159;       // 64-bit float
var k: bool = true;         // Boolean
var l: char = 'A';          // Single character
var m: string = "hello";    // String (RC-managed)
var n: voidptr = null;      // Opaque pointer (for FFI)
```

### Numeric Literals

```whist
var decimal = 42;
var hex = 0xFF;
var binary = 0b1010;
var octal = 0o755;
var float_val = 3.14;
var scientific = 1.5e10;
```

### Tuples

```whist
var t: (i64, string) = (42, "hello");
var pair = (1, 2);               // Type inferred
var x = t[0];                    // Element access
var y = t[1];

// Destructuring
var (a, b) = (10, 20);
var (name, _) = get_pair();      // Discard with _

// Function returning tuple
func find(args: Vec<string>, idx: i64) -> (string, i64) {
    return (args[idx], idx);
}
```

### Arrays (Fixed-Size)

```whist
var arr: [5]i64 = [1, 4, 3, 5, 6];
var arr2 = [10, 20, 30];        // Inferred: [3]i64
var elem = arr[0];               // Bounds-checked
```

### Spans (Views/Slices)

```whist
var arr: [5]i64 = [10, 20, 30, 40, 50];
var s: Span<i64> = arr[:];       // Full span
var s1 = arr[1:4];               // Partial: [20, 30, 40]
var s2 = arr[2:];                // From index
var s3 = arr[:3];                // To index
var count = s.count;             // Length
```

### Vec (Dynamic Array)

```whist
var nums = new Vec<i64>{};               // Empty
var nums = new Vec<i64>{1, 2, 3};        // With elements
var strs = new Vec<string>{"a", "b"};    // Generic

// Mutation
nums.push(4);
nums[0] = 100;
var popped = nums.pop();                 // Option<T>
nums.insert(0, 99);
var removed = nums.remove(0);
var swapped = nums.swap_remove(0);
nums.clear();
nums.reserve(100);
nums.sort();

// Query
var x = nums[0];
var count = nums.count;
var empty = nums.is_empty();
var has = nums.contains(42);
var first = nums.first();                // Option<T>
var last = nums.last();                  // Option<T>

// Functional (see Lambdas section)
var doubled = nums.map(|x| x * 2);      // Vec<K>
var evens = nums.filter(|x| x % 2 == 0); // Vec<T>
var has_big = nums.any(|x| x > 100);    // bool
var all_ok = nums.all(|x| x > 0);       // bool
nums.each(|x| std::println($"{x}"));    // void
var found = nums.find(|x| x == 3);      // Option<T>
nums.extend(other_vec);                  // Append all

// Slicing
var span = nums[1:4];                   // Span<T>
```

### Type Aliases

```whist
type UserId = i64;
type IntBox = Box<i64>;
type StringPair<V> = Pair<string, V>;    // Generic alias
```

### Type Casting

```whist
var code: i32 = 'A' as i32;     // char to int (65)
var ch: char = 65 as char;      // int to char
var wide: i64 = 42 as i64;      // widening
var tag: i64 = color as i64;    // simple enum to int
var ptr: voidptr = p as voidptr; // struct to void pointer
```

## Variables & Constants

```whist
var x = 42;                  // Mutable, type inferred
var y: i32 = 100;            // Mutable, explicit type
const PI = 3.14159;          // Immutable
const MAX: i32 = 1024;       // Immutable, explicit type

private const INTERNAL = 42; // File-local
public const PUBLIC = 99;    // Module-exported (default)
```

### Struct Destructuring

```whist
var {output, error_output, exit_code} = std::exec(cmd);
var {x, y} = point;
var {code, value: val} = info;   // Rename binding
const {x, y} = point;           // Immutable bindings
```

## Strings

### Literals & Escapes

```whist
var s = "hello";
var multi = """
    multi-line string
    leading whitespace stripped
    """;
var esc = "newline\n tab\t null\0 backslash\\\\ quote\" esc\e hex\x41 octal\101";
```

### Interpolation

```whist
var name = "Alice";
var age: i64 = 30;
var msg = $"Hello {name}, age {age}";
var expr = $"sum: {x + y}";
var call = $"len: {s.length()}";
var escaped = $"literal {{braces}}";
// Supported types: i8-i64, u8-u64, f32, f64, bool, char, string
```

### Methods

```whist
var s = "hello world";
s.length()                    // i64
s.contains("world")           // bool
s.starts_with("hello")        // bool
s.ends_with("world")          // bool
s.index_of("o")               // i64 (-1 if not found)
s.split(" ")                  // Vec<string>
s.trim()                      // string
s.trim_start()                // string
s.trim_left()                 // string
s.trim_right()                // string
s.strip_prefix("hello ")      // string
s.pad_right(20, ' ')          // string

// Indexing & slicing
var c = s[0];                 // char
var sub = s[1:4];             // string ("ell")
var from = s[1:];             // string
var to = s[:3];               // string

// Operators
var concat = s1 + s2;         // concatenation
var eq = s1 == s2;            // equality (strcmp)
var lt = s1 < s2;             // lexicographic comparison
```

### StringBuilder

```whist
var sb = new StringBuilder{};
sb.append("hello");
sb.append_char(' ');
sb.append_line("world");
var s = sb.to_string();
var len = sb.len();
sb.clear();
```

## Structs

```whist
struct Point {
    x: i64,
    y: i64,
}

struct Box {
    const v: i64,            // Read-only after construction
    name: string,
}

// Generic struct
struct Pair<K, V> {
    key: K,
    value: V,
}

// Initialization
var p = new Point{x: 10, y: 20};          // Literal form
var p2 = new Point(10, 20);               // Constructor form (requires init)
var b = new Box<i64>{value: 42};           // Generic

// Access
var x = p.x;
p.x = 15;
```

### Methods

```whist
// Mutable method (can modify self)
func (Point) move(dx: i64, dy: i64) {
    self.x += dx;
    self.y += dy;
}

// Immutable method (read-only self)
func (const Point) sum() -> i64 {
    return self.x + self.y;
}

// Constructor
impl Point {
    func init(x: i64, y: i64) {
        self.x = x;
        self.y = y;
    }
}
```

### Generic Methods

```whist
func (Box<T>) get() -> T {
    return self.value;
}

// Method-level type parameters
func (Vec<T>) map<K>(transform: func(T) -> K) -> Vec<K> {
    var result = new Vec<K>{};
    foreach (const elem in self) {
        result.push(transform(elem));
    }
    return result;
}
```

## Enums

### Simple Enums

```whist
enum Color { Red, Green, Blue }

var c = Color::Red;
var val: i64 = c as i64;    // Cast to integer
```

### Enums with Explicit Values

```whist
enum TokenType {
    Eof = -1,
    Plus = 43,
    Minus,                   // Auto-increments: 44
}
```

### Data Enums (Tagged Unions)

```whist
enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

var s = Shape::Circle(3.14);
var r = Shape::Rect(10.0, 20.0);
```

### Generic Enums

```whist
// Defined in prelude (auto-imported)
enum Option<T> { Some(T), None }
enum Result<T, E> { Ok(T), Err(E) }

var opt: Option<i64> = Option::Some(42);
var res: Result<string, i64> = Result::Ok("success");
```

### Pattern Matching

```whist
// Match expression
var area: f64 = match (shape) {
    Circle(r) => 3.14159 * r * r,
    Rect(w, h) => w * h,
    None => 0.0,
};

// Match statement
match (shape) {
    Circle(r) => { std::println($"radius: {r}"); },
    Rect(w, h) => { std::println($"rect: {w}x{h}"); },
    None => {},
}

// Match on values
match (s) {
    "hello" => 1,
    "world" => 2,
    _ => 0,               // Wildcard default
};

// If-is pattern matching
if (opt is Some(v)) {
    std::println($"value: {v}");
}

if (opt is None) {
    std::println("no value");
}

// If-is with guard condition
if (shape is Circle(r) && r > 5.0) {
    std::println("big circle");
}
```

### Option Methods (Prelude)

```whist
var opt: Option<i64> = Option::Some(42);
opt.has_value()              // bool
opt.value()                  // T (panics on None)
opt.value_or(0)              // T with default
opt.expect("msg")            // T (panics with msg on None)
opt.map(|x| x * 2)          // Option<U>
opt.and_then(|x| Option::Some(x + 1))  // Option<U>
opt.unwrap_or_else(|| 0)    // T with lazy default
```

### Result Methods (Prelude)

```whist
var res: Result<i64, string> = Result::Ok(42);
res.is_ok()                  // bool
res.is_err()                 // bool
res.value()                  // T (panics on Err)
res.error()                  // E (panics on Ok)
res.value_or(0)              // T with default
res.map(|x| x * 2)          // Result<U, E>
res.map_err(|e| "wrapped")  // Result<T, F>
```

### Try Operator

```whist
func might_fail() -> Result<i64, string> {
    return Result::Ok(42);
}

func run() -> Result<bool, string> {
    var val = might_fail()?;   // Unwrap or propagate error
    return Result::Ok(val > 0);
}

// Also works with Option
func find(key: i64) -> Option<i64> { ... }

func process() -> Option<bool> {
    var val = find(5)?;        // Unwrap or propagate None
    return Option::Some(val > 0);
}
```

## Control Flow

### If-Else

```whist
if (x > 0) {
    // ...
} else if (x < 0) {
    // ...
} else {
    // ...
}
```

### While Loop

```whist
while (x < 10) {
    x += 1;
}
```

### For Loop

```whist
for (var i = 0; i < 10; i += 1) {
    // ...
}
```

### Foreach (Range)

```whist
foreach (const i in 0..10) { }         // 0 to 9
foreach (const i in 0..20 by 2) { }    // 0, 2, 4, ..., 18
```

### Foreach (Collections)

```whist
foreach (const item in vec) { }        // Over Vec<T>
foreach (const c in "hello") { }       // Over string (yields char)
foreach (const elem in span) { }       // Over Span<T>
```

### Break & Continue

```whist
while (true) {
    if (done) break;
    if (skip) continue;
}
```

### Defer

```whist
func process() {
    defer cleanup();         // Executes at function exit (LIFO order)
    defer close_file();
    // ...
    return;                  // Defers run before return
}
```

## Functions

```whist
func add(a: i64, b: i64) -> i64 {
    return a + b;
}

func greet(name: string) {
    std::println($"hello {name}");   // Implicit void return
}

// Visibility
public func exported() { }
private func internal() { }

// Generic function
func identity<T>(x: T) -> T {
    return x;
}
```

## Lambdas & Closures

```whist
var f = |x: i64| x * 2;                  // Expression body
var g = |a: i64, b: i64| a + b;          // Multiple params
var h = |x: i64| { std::println($"{x}"); }; // Block body
var no_args = || 42;                      // No params

// Type annotation
var f: func(i64) -> i64 = |x| x * 2;

// Closures capture variables
var offset = 10;
var add_offset = |x: i64| x + offset;    // Captures offset

// Common use with Vec
nums.filter(|x| x > 0)
nums.map(|x| x * 2)
nums.any(|x| x > 100)
nums.all(|x| x >= 0)
nums.each(|x| std::println($"{x}"))
nums.find(|x| x == target)
```

## Traits & Implementations

```whist
trait Drop {
    func drop() -> void;
}

trait Eq {
    func eq(other: Self) -> bool;
}

impl Drop for Resource {
    func drop() {
        // Custom cleanup when RC hits 0
    }
}

// Generic trait impl
impl Drop for Box<T> {
    func drop() { }
}

// Inherent impl (no trait)
impl Point {
    func init(x: i64, y: i64) {
        self.x = x;
        self.y = y;
    }
}
```

## Modules & Imports

```whist
import std;                          // Module import
import fs;
import collections;

std::println("qualified access");    // :: for module access

use std::println;                    // Bring into scope
use std::{println, format};          // Grouped
println("unqualified");

include "./helper.w";                // Merge into current namespace
```

## Testing

```whist
test "arithmetic" {
    assert(1 + 1 == 2);
    assert(add(2, 3) == 5);
}

// Run with: w0 test <file.w>
// In normal compilation, test blocks are ignored
```

## Standard Library

### std

```whist
import std;

// I/O
std::print("no newline");
std::println("with newline");
std::eprint("stderr no newline");
std::eprintln("stderr with newline");

// Arguments & control
var args = std::args();              // Vec<string>
std::exit(1);                        // Exit process

// Math
std::abs_i64(-42)                    // i64
std::max_i64(a, b)                   // i64
std::min_i64(a, b)                   // i64

// Conversion
std::parse_i64("42")                 // i64
std::to_string(42)                   // string

// Formatting (varargs builtin)
std::format("x=%d, s=%s", 42, "hi") // string

// Command execution
var result = std::exec("ls -l");
// result.exit_code: i32
// result.output: string
// result.error_output: string

// Destructuring form:
var {output, error_output, exit_code} = std::exec(cmd);
```

### fs

```whist
import fs;

// File convenience API
fs::read_file("path")               // string
fs::write_file("path", "content")
fs::append_file("path", "more")
fs::file_exists("path")             // bool
fs::remove_file("path")
fs::rename_file("old", "new")
fs::file_size("path")               // i64

// Handle-based API
var f = fs::open("file", "r");
var line = fs::read_line(f);
fs::write_string(f, "data");
fs::close(f);

// Directory operations
fs::mkdir("dir");
fs::mkdir_all("path/to/dir");
fs::is_dir("path")                  // bool
fs::is_file("path")                 // bool
var cwd = fs::cwd();
fs::chdir("/path");

// Recursive walk
var files = fs::walk_dir(".");       // Vec<string>

// Path utilities
fs::join_path("dir", "file.txt")
fs::dirname("path/to/file.txt")     // "path/to"
fs::basename("path/to/file.txt")    // "file.txt"
fs::extension("file.txt")           // "txt"
fs::abs_path(".")
```

### collections

```whist
import collections;

// HashMap<K, V> (K must be Hashable)
var map = new HashMap<string, i64>(16);  // With initial capacity
map.set("key", 42);
var val = map.get("key");           // Option<V>
var has = map.has("key");           // bool
map.delete("key");                  // bool
var keys = map.keys();              // Vec<K>

// Set<T> (T must be Hashable)
var set = new Set<string>(16);
set.insert("value");
set.insert_all(vec_of_strings);
var has = set.contains("value");    // bool
set.remove("value");                // bool
var vals = set.values();            // Vec<T>

// Hashable: i32, i64, bool, string
```

### time

```whist
import time;
var ms = time::time_ms();            // i64 (milliseconds since epoch)
```

## Memory Management

Whist uses automatic reference counting. Objects allocated with `new` have a refcount header.

```whist
var p = new Point{x: 1, y: 2};      // refcount = 1
var q = p;                           // refcount = 2 (shared reference)
// When both go out of scope, refcount hits 0, memory freed

// Custom cleanup via Drop trait
impl Drop for Resource {
    func drop() {
        // Called when refcount hits 0, before free
    }
}
```

No manual `free()` calls needed. RC cleanup happens at scope exit and function return.

## Visibility

```whist
public func exported() { }          // Default: module-exported
private func internal() { }         // File-local (static in C)

public struct Visible { }
private struct Hidden { }
```

## Extern (FFI)

```whist
private extern my_lib {
    func c_function(x: i32) -> i32;
    func aliased(x: i32) -> i32 as actual_c_name;
}
```

## Reference Equality

```whist
var a = new Point{x: 1, y: 2};
var b = a;                           // Same reference
var c = new Point{x: 1, y: 2};      // Different reference
assert(sameref(a, b));               // true
assert(!sameref(a, c));              // false
```
