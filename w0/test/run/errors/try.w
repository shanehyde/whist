// Expected: PASS: try_chain
// Expected: PASS: try_option_basic
// Expected: PASS: try_result_basic

enum Result<T, E> {
    Ok(T),
    Err(E),
}

enum Option<T> {
    Some(T),
    None,
}

// --- try_chain ---

func step1() -> Result<i64, string> {
    return Result::Ok(1);
}

func step2(x: i64) -> Result<i64, string> {
    return Result::Ok(x + 1);
}

func step3(x: i64) -> Result<bool, string> {
    return Result::Ok(x > 0);
}

func run_all() -> Result<bool, string> {
    var a = step1()?;
    var b = step2(a)?;
    var c = step3(b)?;
    return Result::Ok(c);
}

test "try_chain" {
}

// --- try_option_basic ---

func find_value(key: i64) -> Option<i64> {
    if (key > 0) {
        return Option::Some(key * 10);
    }
    return Option::None;
}

func process() -> Option<bool> {
    var val = find_value(5)?;
    return Option::Some(val > 0);
}

test "try_option_basic" {
}

// --- try_result_basic ---

func parse_int(s: string) -> Result<i64, string> {
    return Result::Ok(42);
}

func do_work() -> Result<bool, string> {
    var x = parse_int("123")?;
    return Result::Ok(x > 0);
}

test "try_result_basic" {
}
