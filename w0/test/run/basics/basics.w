// Expected: PASS: hello
// Expected: PASS: variables
// Expected: PASS: integers
// Expected: PASS: operators
// Expected: PASS: bitwise_assign
// Expected: PASS: shift_assign
// Expected: PASS: control_flow
// Expected: PASS: functions

// Shared function definitions for the functions test
func add(a: i32, b: i32) -> i32 {
    return a + b;
}

func multiply(x: i32, y: i32) -> i32 {
    return x * y;
}

func factorial(n: i32) -> i32 {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

// Test basic assertion
test "hello" {
    var x = 42;
    assert(x == 42);
}

// Test variable declarations and types
test "variables" {
    // Type inference
    var a = 42;
    var b = 3.14;
    var c = true;
    var d = "hello";
    var e = 'x';

    // Explicit types
    var x: i32 = 100;
    var y: f32 = 2.718;
    var y64: f64 = 2.718;
    var z: bool = false;

    // Constants
    const PI = 3.14159;
    const MAX = 1000;

    // Numeric literals
    var hex = 0xFF;
    var binary = 0b1010;
    var octal = 0o755;
    var scientific = 1.5e10;

    assert(a + x == 142);
}

// Test various integer types
test "integers" {
    // Signed integers
    var a: i8 = 127;
    var b: i16 = 32767;
    var c: i32 = 2147483647;
    var d: i32 = 1234567890;

    // Unsigned integers
    var e: u8 = 255;
    var f: u16 = 65535;
    var g: u32 = 4294967295;

    // Arithmetic operations
    var sum: i32 = c + 1;
    var diff: i16 = b - 100;

    // Bitwise operations
    var masked: u8 = e & 0x0F;
    var shifted: u16 = f >> 8;

    // Mixed operations (result is i64)
    var mixed = a + b;

    // Verify some values
    assert(a == 127);
    assert(e == 255);
    assert(masked == 15);
    assert(shifted == 255);
}

// Test operators
test "operators" {
    var a = 10;
    var b = 3;

    // Arithmetic
    var sum = a + b;
    var diff = a - b;
    var prod = a * b;
    var quot = a / b;
    var rem = a % b;

    // Comparison
    var eq = a == b;
    var ne = a != b;
    var lt = a < b;
    var gt = a > b;
    var le = a <= b;
    var ge = a >= b;

    // Logical
    var and = eq && ne;
    var or = lt || gt;
    var not = !eq;

    // Bitwise
    var band = a & b;
    var bor = a | b;
    var bxor = a ^ b;
    var bnot = ~a;
    var shl = a << 2;
    var shr = a >> 1;

    // Compound assignment
    a += 5;
    a -= 2;
    a *= 3;
    a /= 2;

    assert(a == 19);
}

// Test modulo, bitwise compound assignment operators
test "bitwise_assign" {
    var a: i64 = 17;
    a %= 5;
    assert(a == 2);

    var b: i64 = 0xFF;
    b &= 0x0F;
    assert(b == 0x0F);

    var c: i64 = 0x0F;
    c |= 0xF0;
    assert(c == 0xFF);

    var d: i64 = 0xFF;
    d ^= 0x0F;
    assert(d == 0xF0);
}

// Test shift assignment operators <<= and >>=
test "shift_assign" {
    var a: i32 = 1;
    var b: i32 = 256;

    // Left shift assignment
    a <<= 4;  // a = 1 << 4 = 16

    // Right shift assignment
    b >>= 4;  // b = 256 >> 4 = 16

    // Verify both are equal
    assert(a == b);
    assert(a == 16);
}

// Test control flow statements
test "control_flow" {
    var x = 0;

    // If-else
    if (x == 0) {
        x = 1;
    } else {
        x = 2;
    }

    // While loop
    while (x < 10) {
        x = x + 1;
    }

    // For loop
    for (var i = 0; i < 5; i += 1) {
        x = x + i;
    }

    // Break and continue
    for (var j = 0; j < 100; j += 1) {
        if (j == 50) {
            break;
        }
        if (j % 2 == 0) {
            continue;
        }
        x = x + 1;
    }

    assert(x == 45);
}

// Test function definitions and calls
test "functions" {
    var sum = add(10, 20);
    var product = multiply(5, 6);
    var fact = factorial(5);
    assert(sum + product + fact == 180);
}
