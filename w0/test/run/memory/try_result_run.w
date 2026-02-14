// RC RUNTIME TEST: ? operator propagates errors and unwraps values

import std;

enum Result<T, E> {
    Ok(T),
    Err(E),
}

func succeed(): Result<i64, string> {
    return Result::Ok(42);
}

func fail_it(): Result<i64, string> {
    return Result::Err("bad");
}

func unwrap_ok(): Result<i64, string> {
    var x = succeed()?;
    return Result::Ok(x + 1);
}

func propagate_err(): Result<i64, string> {
    var x = fail_it()?;
    return Result::Ok(x + 1);
}

func main(): i32 {
    var ok_result = unwrap_ok();
    // Should be Ok(43)
    match (ok_result) {
        Ok(val) => {
            if (val != 43) { return 2; }
        },
        Err(e) => { return 1; },
    }

    var err_result = propagate_err();
    // Should be Err("bad") - error was propagated
    match (err_result) {
        Ok(val) => { return 3; },
        Err(e) => {
            // Error was propagated correctly
        },
    }

    return 0;
}
