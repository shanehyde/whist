// Test span bounds checking (this should panic at runtime)
// Expected exit: 1
// Expected stderr: Panic: span index 5 out of bounds
func main(): i32 {
    var arr: [3]i64;
    arr[0] = 1;
    arr[1] = 2;
    arr[2] = 3;
    var s: Span<i64> = arr[:];

    // This should cause a bounds check failure
    var bad = s[5];
    return 0;
}
