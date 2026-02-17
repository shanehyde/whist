// Expected: PASS: closure_basic_capture
// Expected: PASS: closure_multi_capture
// Expected: PASS: closure_returned
// Expected: PASS: closure_nested_transitive_capture
// Expected: PASS: closure_copy_lifetime
// Expected: PASS: closure_uninitialized_then_assign

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

test "closure_nested_transitive_capture" {
    var x: i64 = 42;
    var outer = |a: i64| -> i64 {
        var inner = |b: i64| -> i64 x + a + b;
        return inner(1);
    };
    assert(outer(10) == 53);
}

test "closure_copy_lifetime" {
    var out: func(i64): i64 = null;
    {
        var base: i64 = 40;
        var src = |x: i64| -> i64 x + base;
        var copy = src;
        out = copy;
    }
    assert(out(2) == 42);
}

test "closure_uninitialized_then_assign" {
    var f: func(i64): i64;
    var n: i64 = 2;
    f = |x: i64| -> i64 x + n;
    assert(f(3) == 5);
}
