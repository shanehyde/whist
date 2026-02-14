// Expected: PASS: tuples
// Test tuple types and operations

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
