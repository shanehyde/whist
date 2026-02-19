// Expected: PASS: result_unwrap_or_else
// Expected: PASS: option_unwrap_or_else
// Expected: PASS: result_map
// Expected: PASS: result_map_err
// Expected: PASS: result_and_then
// Expected: PASS: option_map
// Expected: PASS: option_and_then

func make_default(e: string) -> i64 {
    return -1;
}

func make_zero() -> i64 {
    return 0;
}

func times_two(x: i64) -> i64 {
    return x * 2;
}

func to_string_len(s: string) -> i64 {
    return s.length();
}

func safe_div(x: i64) -> Result<i64, string> {
    if (x == 0) {
        return Result::Err("division by zero");
    }
    return Result::Ok(100 / x);
}

func add_ten(x: i64) -> Option<i64> {
    return Option::Some(x + 10);
}

test "result_unwrap_or_else" {
    var ok: Result<i64, string> = Result::Ok(42);
    var v = ok.unwrap_or_else(make_default);
    assert(v == 42);

    var err: Result<i64, string> = Result::Err("fail");
    var v2 = err.unwrap_or_else(make_default);
    assert(v2 == -1);
}

test "option_unwrap_or_else" {
    var some: Option<i64> = Option::Some(10);
    var v = some.unwrap_or_else(make_zero);
    assert(v == 10);

    var none: Option<i64> = Option::None;
    var v2 = none.unwrap_or_else(make_zero);
    assert(v2 == 0);
}

test "result_map" {
    var ok: Result<i64, string> = Result::Ok(21);
    var mapped = ok.map(times_two);
    assert(mapped.is_ok());
    assert(mapped.value() == 42);

    var err: Result<i64, string> = Result::Err("bad");
    var mapped2 = err.map(times_two);
    assert(mapped2.is_err());
    assert(mapped2.error() == "bad");
}

test "result_map_err" {
    var ok: Result<i64, string> = Result::Ok(42);
    var mapped = ok.map_err(to_string_len);
    assert(mapped.is_ok());
    assert(mapped.value() == 42);

    var err: Result<i64, string> = Result::Err("hello");
    var mapped2 = err.map_err(to_string_len);
    assert(mapped2.is_err());
    assert(mapped2.error() == 5);
}

test "result_and_then" {
    var ok: Result<i64, string> = Result::Ok(5);
    var chained = ok.and_then(safe_div);
    assert(chained.is_ok());
    assert(chained.value() == 20);

    var ok2: Result<i64, string> = Result::Ok(0);
    var chained2 = ok2.and_then(safe_div);
    assert(chained2.is_err());
    assert(chained2.error() == "division by zero");

    var err: Result<i64, string> = Result::Err("already bad");
    var chained3 = err.and_then(safe_div);
    assert(chained3.is_err());
    assert(chained3.error() == "already bad");
}

test "option_map" {
    var some: Option<i64> = Option::Some(21);
    var mapped = some.map(times_two);
    assert(mapped.has_value());
    assert(mapped.value() == 42);

    var none: Option<i64> = Option::None;
    var mapped2 = none.map(times_two);
    assert(!mapped2.has_value());
}

test "option_and_then" {
    var some: Option<i64> = Option::Some(5);
    var chained = some.and_then(add_ten);
    assert(chained.has_value());
    assert(chained.value() == 15);

    var none: Option<i64> = Option::None;
    var chained2 = none.and_then(add_ten);
    assert(!chained2.has_value());
}
