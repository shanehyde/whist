// Test: autoboxing syntax var ^name = value

import std;

// Expected: PASS: basic autobox
// Expected: PASS: typed autobox
// Expected: PASS: arithmetic
// Expected: PASS: assign through
// Expected: PASS: sharing
// Expected: PASS: bool autobox
// Expected: PASS: float autobox
// Expected: PASS: const autobox
// Expected: 8 passed, 0 failed, 8 total

func increment(b: Box<i64>) -> void {
    b = b + 1;
}

test "basic autobox" {
    var ^x = 42;
    assert(x == 42);
}

test "typed autobox" {
    var ^y: i64 = 10;
    assert(y == 10);
}

test "arithmetic" {
    var ^a = 5;
    var ^b = 3;
    assert(a + b == 8);
}

test "assign through" {
    var ^a = 5;
    a = 100;
    assert(a == 100);
}

test "sharing" {
    var ^total = 0;
    increment(total);
    increment(total);
    assert(total == 2);
}

test "bool autobox" {
    var ^flag = true;
    assert(flag == true);
    flag = false;
    assert(flag == false);
}

test "float autobox" {
    var ^pi = 3.14;
    assert(pi > 3.0);
}

test "const autobox" {
    const ^c = 99;
    assert(c == 99);
}

func main() -> i32 {
    return 0;
}
