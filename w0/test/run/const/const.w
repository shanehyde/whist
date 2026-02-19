// Expected: PASS: const_binding_field_const_method
// Expected: PASS: const_field_valid
// Expected: PASS: const_params
// Expected: PASS: const_return_type
// Expected: PASS: const_struct_const_method
// Expected: PASS: const_vec_readonly
// Expected: PASS: toplevel_const

// Shared struct definitions

struct Point {
    x: i64,
    y: i64,
}

struct Wrap {
    p: Point,
}

struct Box {
    const v: i64,
    name: string,
}

// Shared method definitions

func (const Point) sum() -> i64 {
    return self.x + self.y;
}

// Function definitions for const_params

func add(const a: i64, const b: i64) -> i64 {
    return a + b;
}

func mixed(const readonly: i32, mutable: i32) -> i32 {
    mutable = mutable + 1;
    return readonly + mutable;
}

func identity(const x: i32) -> i32 {
    return x;
}

// Function definitions for const_return_type

func make_view() -> const Vec<i64> {
    return new Vec<i64>{1, 2, 3};
}

// Top-level const declarations

const MAX = 1024;
const PI = 3.14159;
const GREETING = "hello";
const DEBUG = false;
const MAX_I32: i32 = 1024;
private const INTERNAL = 42;
const KEYWORDS: [3]string = ["if", "else", "while"];

// Tests

// Valid: const methods remain callable through fields of const bindings
test "const_binding_field_const_method" {
    const w = new Wrap { p: new Point { x: 10, y: 20 } };
    var s = w.p.sum();
    var x = w.p.x;
}

// Valid: const field can be set at construction and read
test "const_field_valid" {
    var b = new Box { v: 42, name: "hello" };
    var x = b.v;
    b.name = "world";
}

// Test const parameters in function declarations
test "const_params" {
    var sum = add(10, 20);
    var result = mixed(5, 10);
    var val = identity(42);
}

// Test that const-qualified return types parse and type-check
test "const_return_type" {
    const v = make_view();
    var n = v.count;
    assert(n >= 0);
}

// Test that const methods can be called on const struct bindings
test "const_struct_const_method" {
    const p = new Point {x: 10, y: 20};
    var s = p.sum();
}

// Test that readonly operations on const Vec are allowed
test "const_vec_readonly" {
    const v = new Vec<i64>{10, 20, 30};
    var c = v.count;
    var x = v[0];
}

// Test top-level const declarations
test "toplevel_const" {
    var x = MAX;
    var y = PI;
    var s = GREETING;
    var d = DEBUG;
    var m = MAX_I32;
    var i = INTERNAL;
}
