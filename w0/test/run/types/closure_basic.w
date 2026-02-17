// Expected: PASS: closure_basic_capture
// Expected: PASS: closure_multi_capture
// Expected: PASS: closure_returned

func apply(f: func(i64): i64, x: i64): i64 {
    return f(x);
}

test "closure_basic_capture" {
    var offset: i64 = 10;
    var f = |x: i64| -> i64 x + offset;
    assert(apply(f, 5) == 15);
    assert(apply(f, 0) == 10);
}

test "closure_multi_capture" {
    var a: i64 = 3;
    var b: i64 = 7;
    var f = |x: i64| -> i64 x + a + b;
    assert(apply(f, 0) == 10);
    assert(apply(f, 5) == 15);
}

test "closure_returned" {
    var n: i64 = 100;
    var adder = |x: i64| -> i64 x + n;
    var result = adder(42);
    assert(result == 142);
}
