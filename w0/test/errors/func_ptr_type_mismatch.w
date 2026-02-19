// Expected error: expected 'func(i64) -> i64'
func add(a: i64, b: i64) -> i64 {
    return a + b;
}

func main() -> i32 {
    var fp: func(i64) -> i64 = add;
    return 0;
}
