// Expected: PASS: if_let_some
// Expected: PASS: if_let_none
// Expected: PASS: if_let_result
// Expected: PASS: if_let_chain
// Expected: PASS: if_let_no_bindings
// Expected: PASS: if_let_multi_bind

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

test "if_let_some" {
    var opt: Option<i64> = Option::Some(42);
    var result: i64 = 0;

    if let Some(v) = opt {
        result = v;
    } else {
        result = -1;
    }
    assert(result == 42);

    // Test with None
    var empty: Option<i64> = Option::None;
    if let Some(v) = empty {
        result = v;
    } else {
        result = -1;
    }
    assert(result == -1);
}

test "if_let_none" {
    var opt: Option<i64> = Option::None;
    var is_none = false;

    if let None = opt {
        is_none = true;
    }
    assert(is_none);

    // Some case should not match None
    var some: Option<i64> = Option::Some(10);
    is_none = false;
    if let None = some {
        is_none = true;
    }
    assert(!is_none);
}

test "if_let_result" {
    var res: Result<i64, string> = Result::Ok(100);
    var value: i64 = 0;

    if let Ok(v) = res {
        value = v;
    } else {
        value = -1;
    }
    assert(value == 100);

    // Test Err case
    var err: Result<i64, string> = Result::Err("oops");
    if let Ok(v) = err {
        value = v;
    } else {
        value = -1;
    }
    assert(value == -1);

    // Match on Err variant
    var msg = "";
    if let Err(e) = err {
        msg = e;
    }
    assert(msg == "oops");
}

test "if_let_chain" {
    var opt: Option<i64> = Option::None;
    var res: Result<i64, string> = Result::Ok(200);
    var value: i64 = 0;

    if let Some(v) = opt {
        value = v;
    } else if let Ok(v) = res {
        value = v;
    } else {
        value = -1;
    }
    assert(value == 200);

    // Both fail
    var err: Result<i64, string> = Result::Err("fail");
    if let Some(v) = opt {
        value = v;
    } else if let Ok(v) = err {
        value = v;
    } else {
        value = -999;
    }
    assert(value == -999);
}

test "if_let_no_bindings" {
    var shape: Shape = Shape::None;
    var result: i64 = 0;

    if let None = shape {
        result = 1;
    } else {
        result = 2;
    }
    assert(result == 1);

    var circle: Shape = Shape::Circle(5.0);
    if let None = circle {
        result = 1;
    } else {
        result = 2;
    }
    assert(result == 2);
}

test "if_let_multi_bind" {
    var shape: Shape = Shape::Rect(3.0, 4.0);
    var area: f64 = 0.0;

    if let Rect(w, h) = shape {
        area = w * h;
    }
    assert(area == 12.0);

    if let Circle(r) = shape {
        area = r;
    } else {
        area = -1.0;
    }
    assert(area == -1.0);
}
