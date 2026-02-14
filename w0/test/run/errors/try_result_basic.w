// Expected: PASS: try_result_basic
// Test ? operator on Result type

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func parse_int(s: string): Result<i64, string> {
    return Result::Ok(42);
}

func do_work(): Result<bool, string> {
    var x = parse_int("123")?;
    return Result::Ok(x > 0);
}

test "try_result_basic" {
}
