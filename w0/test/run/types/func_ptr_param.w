// Expected: PASS: func_ptr_param

func apply(f: func(i64): i64, x: i64): i64 {
    return f(x);
}

func twice(x: i64): i64 {
    return x * 2;
}

test "func_ptr_param" {
    var result = apply(twice, 5);
    assert(result == 10);
}
