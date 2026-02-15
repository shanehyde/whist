// Expected: PASS: prelude_basic
// Expected: PASS: prelude_value_methods

func maybe_parse(s: string): Option<i64> {
    if (s == "42") {
        return Option::Some(42);
    }
    return Option::None;
}

func try_parse(s: string): Result<i64, string> {
    if (s == "42") {
        return Result::Ok(42);
    }
    return Result::Err("bad");
}

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

test "prelude_basic" {
    var opt = maybe_parse("42");
    match (opt) {
        Some(val) => {
            assert(val == 42);
        },
        None => {
            assert(false);
        },
    }

    var res = try_parse("abc");
    match (res) {
        Ok(val) => {
            assert(false);
        },
        Err(msg) => {
            assert(msg == "bad");
        },
    }
}

test "prelude_value_methods" {
}
