// Expected error: '?' used in function returning 'i64'
// Test ? in function with non-Result/Option return type

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func get_value() -> Result<i64, string> {
    return Result::Ok(42);
}

func bad_return() -> i64 {
    var x = get_value()?;
    return x;
}

func main() -> i32 {
    return 0;
}
