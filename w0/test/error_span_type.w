// Error test: Span with wrong number of type args
func main(): i32 {
    var s: Span<i64, i32>;  // Should error - Span takes 1 arg
    return 0;
}
