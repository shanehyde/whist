// Expected error: expects 2 type arguments, got 1

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func main() -> i32 {
    var x: Result<i64> = Result::Ok(42);
    return 0;
}
