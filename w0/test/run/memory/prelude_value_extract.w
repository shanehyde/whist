// Expected: PASS: prelude_value_extract
// Test Option/Result extraction methods at runtime

test "prelude_value_extract" {
    // Option.value() on Some
    var opt_some: Option<i64> = Option::Some(42);
    assert(opt_some.value() == 42);

    // Option.expect() on Some
    assert(opt_some.expect("unreachable") == 42);

    // Option.value_or() on Some returns inner value
    assert(opt_some.value_or(99) == 42);

    // Option.value_or() on None returns default
    var opt_none: Option<i64> = Option::None;
    assert(opt_none.value_or(99) == 99);

    // Result.value() on Ok
    var res_ok: Result<i64, string> = Result::Ok(100);
    assert(res_ok.value() == 100);

    // Result.expect() on Ok
    assert(res_ok.expect("unreachable") == 100);

    // Result.value_or() on Ok returns inner value
    assert(res_ok.value_or(0) == 100);

    // Result.value_or() on Err returns default
    var res_err: Result<i64, string> = Result::Err("bad");
    assert(res_err.value_or(0) == 0);

    // Result.error() on Err
    var res_err2: Result<i64, string> = Result::Err("error_msg");
    var e: string = res_err2.error();

    // Option with string type
    var opt_str: Option<string> = Option::Some("hello");
    assert(opt_str.value_or("default") == "hello");

    var opt_str_none: Option<string> = Option::None;
    assert(opt_str_none.value_or("default") == "default");
}
