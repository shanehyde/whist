// Expected: PASS: std_result
// Expected: PASS: mismatched_result
// Test Result<T, E> from prelude — return Ok and Err, match on both

func parse_number(s: string) -> Result<i64, string> {
    if (s == "42") {
        return Result::Ok(42);
    }
    return Result::Err("not a number");
}

test "std_result" {
    // Test Ok case
    var r1 = parse_number("42");
    match (r1) {
        Ok(val) => {
            assert(val == 42);
        },
        Err(msg) => {
            assert(false);
        },
    }

    // Test Err case
    var r2 = parse_number("abc");
    match (r2) {
        Ok(val) => {
            assert(false);
        },
        Err(msg) => {
            assert(msg == "not a number");
        },
    }
}

test "mismatched_result" {

    var res: Result<string, string> = Result::Ok("hello");

    match (res) {
        Result::Ok(val) => {
            assert(val == "hello");
        },
        Result::Err(msg) => {
            assert(false);
        },
    }
}
