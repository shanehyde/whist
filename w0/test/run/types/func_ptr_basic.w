// Expected: PASS: func_ptr_basic

func add(a: i64, b: i64) -> i64 {
    return a + b;
}

test "func_ptr_basic" {
    var fp: func(i64, i64) -> i64 = add;
    var result = fp(2, 3);
    assert(result == 5);
}
