// Match on string
func match_string(s: string) -> i64 {
    match (s) {
        "hello" => { return 1; },
        "world" => { return 2; },
        _ => { return 0; },
    }
    return 0;
}

// Match on i64
func match_int(x: i64) -> i64 {
    match (x) {
        1 => { return 10; },
        2 => { return 20; },
        -1 => { return -10; },
        _ => { return 0; },
    }
    return 0;
}

// Match on char
func match_char(c: char) -> i64 {
    match (c) {
        'a' => { return 1; },
        'b' => { return 2; },
        _ => { return 0; },
    }
    return 0;
}

// Match on bool
func match_bool(b: bool) -> i64 {
    match (b) {
        true => { return 1; },
        false => { return 0; },
    }
    return 0;
}

// Match on f64
func match_float(f: f64) -> i64 {
    match (f) {
        1.0 => { return 1; },
        -2.5 => { return 2; },
        _ => { return 0; },
    }
    return 0;
}

// Match expression (returns value)
func match_expr(s: string) -> i64 {
    var result = match(s) {
        "hello" => 1,
        "world" => 2,
        _ => 0,
    };
    return result;
}

// Match statement without wildcard (allowed for statements)
func match_no_wildcard(x: i64) -> i64 {
    match (x) {
        1 => { return 10; },
        2 => { return 20; },
    }
    return 0;
}

func main() -> i32 {
    return 0;
}
