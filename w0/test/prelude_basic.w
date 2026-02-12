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

func main(): i32 {
    var opt = maybe_parse("42");
    match (opt) {
        Some(val) => {
            if (val != 42) {
                return 1;
            }
        },
        None => {
            return 2;
        },
    }

    var res = try_parse("abc");
    match (res) {
        Ok(val) => {
            return 3;
        },
        Err(msg) => {
            if (msg != "bad") {
                return 4;
            }
        },
    }

    return 0;
}
