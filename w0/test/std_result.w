// Test std.Result<T, E> — return Ok and Err, match on both
import std;

func parse_number(s: string): Result<i64, string> {
    if (s == "42") {
        return Result::Ok(42);
    }
    return Result::Err("not a number");
}

func main(): i32 {
    // Test Ok case
    var r1 = parse_number("42");
    match (r1) {
        Ok(val) => {
            if (val != 42) {
                return 1;
            }
        },
        Err(msg) => {
            return 2;
        },
    }

    // Test Err case
    var r2 = parse_number("abc");
    match (r2) {
        Ok(val) => {
            return 3;
        },
        Err(msg) => {
            if (msg != "not a number") {
                return 4;
            }
        },
    }

    return 0;
}
