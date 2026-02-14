// Test Option/Result query methods provided by prelude

func main(): i32 {
    var opt_some: Option<i64> = Option::Some(42);
    var opt_none: Option<i64> = Option::None;

    if (!opt_some.has_value()) {
        return 1;
    }
    if (opt_none.has_value()) {
        return 2;
    }

    var res_ok: Result<i64, string> = Result::Ok(42);
    var res_err: Result<i64, string> = Result::Err("bad");

    if (!res_ok.has_value()) {
        return 3;
    }
    if (res_err.has_value()) {
        return 4;
    }

    if (!res_ok.is_ok()) {
        return 5;
    }
    if (res_err.is_ok()) {
        return 6;
    }

    if (res_ok.is_err()) {
        return 7;
    }
    if (!res_err.is_err()) {
        return 8;
    }

    return 0;
}
