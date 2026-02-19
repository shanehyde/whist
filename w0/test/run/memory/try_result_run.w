// Expected: PASS: try_result_run
// RC RUNTIME TEST: ? operator propagates errors and unwraps values

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func succeed() -> Result<i64, string> {
    return Result::Ok(42);
}

func fail_it() -> Result<i64, string> {
    return Result::Err("bad");
}

func unwrap_ok() -> Result<i64, string> {
    var x = succeed()?;
    return Result::Ok(x + 1);
}

func propagate_err() -> Result<i64, string> {
    var x = fail_it()?;
    return Result::Ok(x + 1);
}

test "try_result_run" {
    var ok_result = unwrap_ok();
    // Should be Ok(43)
    match (ok_result) {
        Ok(val) => {
            assert(val == 43);
        },
        Err(e) => { assert(false); },
    }

    var err_result = propagate_err();
    // Should be Err("bad") - error was propagated
    match (err_result) {
        Ok(val) => { assert(false); },
        Err(e) => {
            // Error was propagated correctly
        },
    }
}
