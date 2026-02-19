// Expected: PASS: if_let_some
// Expected: PASS: if_let_none
// Expected: PASS: if_let_result
// Expected: PASS: if_let_chain
// Expected: PASS: if_let_no_bindings
// Expected: PASS: if_let_multi_bind
// Expected: PASS: if_is_bare_check
// Expected: PASS: if_is_extra_cond

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

test "if_let_some" {
    var opt: Option<i64> = Option::Some(42);
    var result: i64 = 0;

    if (opt is Some(v)) {
        result = v;
    } else {
        result = -1;
    }
    assert(result == 42);

    // Test with None
    var empty: Option<i64> = Option::None;
    if (empty is Some(v)) {
        result = v;
    } else {
        result = -1;
    }
    assert(result == -1);
}

test "if_let_none" {
    var opt: Option<i64> = Option::None;
    var is_none = false;

    if (opt is None) {
        is_none = true;
    }
    assert(is_none);

    // Some case should not match None
    var some: Option<i64> = Option::Some(10);
    is_none = false;
    if (some is None) {
        is_none = true;
    }
    assert(!is_none);
}

test "if_let_result" {
    var res: Result<i64, string> = Result::Ok(100);
    var value: i64 = 0;

    if (res is Ok(v)) {
        value = v;
    } else {
        value = -1;
    }
    assert(value == 100);

    // Test Err case
    var err: Result<i64, string> = Result::Err("oops");
    if (err is Ok(v)) {
        value = v;
    } else {
        value = -1;
    }
    assert(value == -1);

    // Match on Err variant
    var msg = "";
    if (err is Err(e)) {
        msg = e;
    }
    assert(msg == "oops");
}

test "if_let_chain" {
    var opt: Option<i64> = Option::None;
    var res: Result<i64, string> = Result::Ok(200);
    var value: i64 = 0;

    if (opt is Some(v)) {
        value = v;
    } else if (res is Ok(v)) {
        value = v;
    } else {
        value = -1;
    }
    assert(value == 200);

    // Both fail
    var err: Result<i64, string> = Result::Err("fail");
    if (opt is Some(v)) {
        value = v;
    } else if (err is Ok(v)) {
        value = v;
    } else {
        value = -999;
    }
    assert(value == -999);
}

test "if_let_no_bindings" {
    var shape: Shape = Shape::None;
    var result: i64 = 0;

    if (shape is None) {
        result = 1;
    } else {
        result = 2;
    }
    assert(result == 1);

    var circle: Shape = Shape::Circle(5.0);
    if (circle is None) {
        result = 1;
    } else {
        result = 2;
    }
    assert(result == 2);
}

test "if_let_multi_bind" {
    var shape: Shape = Shape::Rect(3.0, 4.0);
    var area: f64 = 0.0;

    if (shape is Rect(w, h)) {
        area = w * h;
    }
    assert(area == 12.0);

    if (shape is Circle(r)) {
        area = r;
    } else {
        area = -1.0;
    }
    assert(area == -1.0);
}

test "if_is_bare_check" {
    // Bare type check: no bindings even though variant has data
    var opt: Option<i64> = Option::Some(42);
    var is_some = false;

    if (opt is Some) {
        is_some = true;
    }
    assert(is_some);

    var empty: Option<i64> = Option::None;
    is_some = false;
    if (empty is Some) {
        is_some = true;
    }
    assert(!is_some);
}

test "if_is_extra_cond" {
    // && condition with bindings
    var opt: Option<i64> = Option::Some(42);
    var result: i64 = 0;

    if (opt is Some(v) && v > 10) {
        result = v;
    } else {
        result = -1;
    }
    assert(result == 42);

    // && condition fails
    var small: Option<i64> = Option::Some(3);
    if (small is Some(v) && v > 10) {
        result = v;
    } else {
        result = -1;
    }
    assert(result == -1);

    // && condition without else
    result = 0;
    if (opt is Some(v) && v == 42) {
        result = 99;
    }
    assert(result == 99);
}
