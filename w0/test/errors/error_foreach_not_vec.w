// Expected error: Foreach collection must be Vec<T>, Span<T>, or string

func main(): i32 {
    var x: i64 = 42;
    foreach (const item in x) {
        x = item;
    }
    return 0;
}
