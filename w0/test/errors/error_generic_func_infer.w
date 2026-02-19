func impossible<T>(x: i64) -> T {
    return x;
}

func main() -> i32 {
    // Expected error: Cannot infer type parameter 'T' for generic function 'impossible'
    var a = impossible(42);
    return 0;
}
