// Test Option/Result extraction methods at runtime

func main(): i32 {
    // Option.value() on Some
    var opt_some: Option<i64> = Option::Some(42);
    if (opt_some.value() != 42) {
        return 1;
    }

    // Option.expect() on Some
    if (opt_some.expect("unreachable") != 42) {
        return 2;
    }

    // Option.value_or() on Some returns inner value
    if (opt_some.value_or(99) != 42) {
        return 3;
    }

    // Option.value_or() on None returns default
    var opt_none: Option<i64> = Option::None;
    if (opt_none.value_or(99) != 99) {
        return 4;
    }

    // Result.value() on Ok
    var res_ok: Result<i64, string> = Result::Ok(100);
    if (res_ok.value() != 100) {
        return 5;
    }

    // Result.expect() on Ok
    if (res_ok.expect("unreachable") != 100) {
        return 6;
    }

    // Result.value_or() on Ok returns inner value
    if (res_ok.value_or(0) != 100) {
        return 7;
    }

    // Result.value_or() on Err returns default
    var res_err: Result<i64, string> = Result::Err("bad");
    if (res_err.value_or(0) != 0) {
        return 8;
    }

    // Result.error() on Err
    var res_err2: Result<i64, string> = Result::Err("error_msg");
    var e: string = res_err2.error();

    // Option with string type
    var opt_str: Option<string> = Option::Some("hello");
    if (opt_str.value_or("default") != "hello") {
        return 10;
    }

    var opt_str_none: Option<string> = Option::None;
    if (opt_str_none.value_or("default") != "default") {
        return 11;
    }

    return 0;
}
