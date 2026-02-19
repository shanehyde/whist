// Error test: Span with wrong number of type args
// Expected error: Span requires exactly 1 type argument, got 2

func main() -> i32 {
    var s: Span<i64, i32>;  // Should error - Span takes 1 arg
    return 0;
}
