// Expected error: '?' operator requires a Result or Option type
// Test ? on non-enum type

func foo() -> i64 {
    var x: i64 = 42;
    var y = x?;
    return y;
}

func main() -> i32 {
    return 0;
}
