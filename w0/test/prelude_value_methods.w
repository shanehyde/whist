// Test type-checking for Option/Result extraction methods from prelude

func test_option(): i32 {
    var opt_some: Option<i64> = Option::Some(42);
    var opt_none: Option<i64> = Option::None;

    // value() returns T
    var v1: i64 = opt_some.value();

    // expect() returns T
    var v2: i64 = opt_some.expect("should not panic");

    // value_or() returns T
    var v3: i64 = opt_none.value_or(99);

    return 0;
}

func test_result(): i32 {
    var res_ok: Result<i64, string> = Result::Ok(42);
    var res_err: Result<i64, string> = Result::Err("bad");

    // value() returns T
    var v1: i64 = res_ok.value();

    // expect() returns T
    var v2: i64 = res_ok.expect("should not panic");

    // value_or() returns T
    var v3: i64 = res_err.value_or(99);

    // error() returns E
    var e1: string = res_err.error();

    return 0;
}

func test_panic(): void {
    // panic is available without import
    // (not calling it here since --check only type-checks)
}

func main(): i32 {
    return 0;
}
