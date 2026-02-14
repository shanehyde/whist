// Expected: PASS: prelude_result_option_queries
// Test Option/Result query methods provided by prelude

test "prelude_result_option_queries" {
    var opt_some: Option<i64> = Option::Some(42);
    var opt_none: Option<i64> = Option::None;

    assert(opt_some.has_value());
    assert(!opt_none.has_value());

    var res_ok: Result<i64, string> = Result::Ok(42);
    var res_err: Result<i64, string> = Result::Err("bad");

    assert(res_ok.has_value());
    assert(!res_err.has_value());

    assert(res_ok.is_ok());
    assert(!res_err.is_ok());

    assert(!res_ok.is_err());
    assert(res_err.is_err());
}
