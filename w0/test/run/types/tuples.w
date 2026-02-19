// Expected: PASS: tuples
// Expected: PASS: tuples with function return
// Test tuple types and operations

func beep() -> (i64, Vec<string>) {
    return (42, new Vec<string>{ "hello", "world" });
}


test "tuples" {
    // Basic tuple literal with type annotation
    var t: (i64, string) = (42, "hello");

    // Tuple element access
    var x: i64 = t[0];
    assert(x == 42);

    // Type-inferred tuple
    var pair = (1, 2);
    assert(pair[0] == 1);
    assert(pair[1] == 2);

    // Destructuring
    var (a, b) = (10, 20);
    assert(a == 10);
    assert(b == 20);

    // Verify values via sum
    assert(x + pair[0] + pair[1] + a + b == 75);
}

test "tuples with function return" {
    var (num, vec) = beep();
    assert(num == 42);
    assert(vec.count == 2);
    assert(vec[0] == "hello");
    assert(vec[1] == "world");
}
