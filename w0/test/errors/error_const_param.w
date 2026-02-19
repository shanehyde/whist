// ERROR TEST: Assignment to const parameter
// Expected error: Cannot assign to const 'x'

func bad(const x: i32) -> i32 {
    x = 42;
    return x;
}

func main() -> i32 {
    return bad(10);
}
