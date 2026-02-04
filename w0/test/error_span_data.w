// Error test: accessing .data field on span should fail
func main(): i32 {
    var arr: [5]i64;
    arr[0] = 1;
    var s: Span<i64> = arr[:];
    var ptr = s.data;  // Should error - data is private
    return 0;
}
