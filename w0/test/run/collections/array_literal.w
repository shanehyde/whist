// Test array literal syntax
func main(): i32 {
    // Array literal with explicit type
    var arr: [5]i64 = [1, 4, 3, 5, 6];

    if (arr[0] != 1) { return 1; }
    if (arr[1] != 4) { return 2; }
    if (arr[2] != 3) { return 3; }
    if (arr[3] != 5) { return 4; }
    if (arr[4] != 6) { return 5; }

    // Array literal with type inference
    var arr2 = [10, 20, 30];
    if (arr2[0] != 10) { return 6; }
    if (arr2[1] != 20) { return 7; }
    if (arr2[2] != 30) { return 8; }

    // Array literal used with span
    var s: Span<i64> = arr[:];
    if (s.count != 5) { return 9; }
    if (s[2] != 3) { return 10; }

    return 0;
}
