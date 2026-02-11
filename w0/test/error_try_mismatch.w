// Expected error: '?' error type mismatch
// Test ? with mismatched error types

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func get_int(): Result<i64, i64> {
    return Result::Ok(42);
}

func wrapper(): Result<bool, string> {
    var x = get_int()?;
    return Result::Ok(x > 0);
}

func main(): i32 {
    return 0;
}
