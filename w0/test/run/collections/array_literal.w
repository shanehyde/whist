// Expected: PASS: array_literal
// Test array literal syntax

test "array_literal" {
    // Array literal with explicit type
    var arr: [5]i64 = [1, 4, 3, 5, 6];

    assert(arr[0] == 1);
    assert(arr[1] == 4);
    assert(arr[2] == 3);
    assert(arr[3] == 5);
    assert(arr[4] == 6);

    // Array literal with type inference
    var arr2 = [10, 20, 30];
    assert(arr2[0] == 10);
    assert(arr2[1] == 20);
    assert(arr2[2] == 30);

    // Array literal used with span
    var s: Span<i64> = arr[:];
    assert(s.count == 5);
    assert(s[2] == 3);
}
