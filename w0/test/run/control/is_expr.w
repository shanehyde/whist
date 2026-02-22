// Expected: PASS: is_standalone_bool
// Expected: PASS: is_chained_bindings
// Expected: PASS: while_chained_is
// Expected: PASS: is_in_var_decl

enum Mood {
    Happy(string),
    Sad,
}

enum Wrapper {
    Inner(i64, i64),
    Empty,
}

test "is_standalone_bool" {
    // is expression without bindings returns bool
    var opt: Option<i64> = Option::Some(42);
    var b: bool = opt is Some;
    assert(b);

    var c: bool = opt is None;
    assert(!c);

    var none: Option<i64> = Option::None;
    assert(none is None);
    assert(!(none is Some));

    // Qualified enum name
    assert(opt is Option::Some);
    assert(none is Option::None);

    // Result type
    var res: Result<i64, string> = Result::Ok(100);
    assert(res is Ok);
    assert(!(res is Err));

    // Simple enum (no data)
    var mood: Mood = Mood::Sad;
    assert(mood is Sad);
    assert(!(mood is Happy));
}

test "is_chained_bindings" {
    // Chained is with multiple binding extractions on different types
    var opt: Option<i64> = Option::Some(42);
    var wrap: Wrapper = Wrapper::Inner(10, 20);

    var value: i64 = 0;
    if (opt is Some(v) && wrap is Inner(a, b)) {
        value = v + a + b;
    }
    assert(value == 72);

    // Chain fails on first is
    var none: Option<i64> = Option::None;
    value = -1;
    if (none is Some(v) && wrap is Inner(a, b)) {
        value = v + a + b;
    }
    assert(value == -1);

    // Chain fails on second is
    var empty: Wrapper = Wrapper::Empty;
    value = -1;
    if (opt is Some(v) && empty is Inner(a, b)) {
        value = v + a + b;
    }
    assert(value == -1);

    // Chain with non-is bool condition in middle
    value = 0;
    if (opt is Some(v) && v > 10 && v < 100) {
        value = v;
    }
    assert(value == 42);

    // Chained is with else
    value = 0;
    if (none is Some(v) && v > 0) {
        value = v;
    } else {
        value = -999;
    }
    assert(value == -999);
}

test "while_chained_is" {
    // While with chained is bindings
    var items = new Vec<Result<i64, string>>{
        Result::Ok(1),
        Result::Ok(2),
        Result::Ok(3),
        Result::Err("stop"),
    };

    var sum: i64 = 0;
    var i: i64 = 0;
    while (items[i] is Ok(v) && v > 0) {
        sum += v;
        i += 1;
    }
    assert(sum == 6);
}

test "is_in_var_decl" {
    // Using is in variable declarations and general expressions
    var opt: Option<i64> = Option::Some(42);
    var is_some: bool = opt is Some;
    var is_none: bool = opt is None;

    assert(is_some);
    assert(!is_none);

    // is with logical operators (no bindings)
    assert(opt is Some && !(opt is None));
    assert(!(opt is None) || opt is Some);
}
