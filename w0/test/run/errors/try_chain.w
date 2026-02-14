// Expected: PASS: try_chain
// Test multiple ? operators in one function and chained with calls

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func step1(): Result<i64, string> {
    return Result::Ok(1);
}

func step2(x: i64): Result<i64, string> {
    return Result::Ok(x + 1);
}

func step3(x: i64): Result<bool, string> {
    return Result::Ok(x > 0);
}

func run_all(): Result<bool, string> {
    var a = step1()?;
    var b = step2(a)?;
    var c = step3(b)?;
    return Result::Ok(c);
}

test "try_chain" {
}
