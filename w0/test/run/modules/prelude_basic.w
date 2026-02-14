// Expected: PASS: prelude_basic
// Test that prelude types are available without import or local definition

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
