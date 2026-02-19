// Expected: PASS: match string
// Expected: PASS: match int
// Expected: PASS: match char
// Expected: PASS: match bool
// Expected: PASS: match float
// Expected: PASS: match expr
// Expected: PASS: match negative

import std;

func match_string(s: string) -> i64 {
    match (s) {
        "hello" => { return 1; },
        "world" => { return 2; },
        _ => { return 0; },
    }
    return -1;
}

func match_int(x: i64) -> i64 {
    match (x) {
        1 => { return 10; },
        2 => { return 20; },
        _ => { return 0; },
    }
    return -1;
}

func match_char(c: char) -> i64 {
    match (c) {
        'a' => { return 1; },
        'b' => { return 2; },
        _ => { return 0; },
    }
    return -1;
}

func match_bool(b: bool) -> i64 {
    match (b) {
        true => { return 1; },
        false => { return 0; },
    }
    return -1;
}

func match_float(f: f64) -> i64 {
    match (f) {
        1.0 => { return 1; },
        2.5 => { return 2; },
        _ => { return 0; },
    }
    return -1;
}

func match_expr(s: string) -> i64 {
    var result = match(s) {
        "hello" => 1,
        "world" => 2,
        _ => 0,
    };
    return result;
}

func match_negative(x: i64) -> i64 {
    match (x) {
        -1 => { return 100; },
        -2 => { return 200; },
        _ => { return 0; },
    }
    return -1;
}

test "match string" {
    assert(match_string("hello") == 1);
    assert(match_string("world") == 2);
    assert(match_string("other") == 0);
}

test "match int" {
    assert(match_int(1) == 10);
    assert(match_int(2) == 20);
    assert(match_int(3) == 0);
}

test "match char" {
    assert(match_char('a') == 1);
    assert(match_char('b') == 2);
    assert(match_char('z') == 0);
}

test "match bool" {
    assert(match_bool(true) == 1);
    assert(match_bool(false) == 0);
}

test "match float" {
    assert(match_float(1.0) == 1);
    assert(match_float(2.5) == 2);
    assert(match_float(9.9) == 0);
}

test "match expr" {
    assert(match_expr("hello") == 1);
    assert(match_expr("world") == 2);
    assert(match_expr("nope") == 0);
}

test "match negative" {
    assert(match_negative(-1) == 100);
    assert(match_negative(-2) == 200);
    assert(match_negative(0) == 0);
}
