// Expected: PASS: func_ptr_inferred

func greet(): void {
    return;
}

func add(a: i64, b: i64): i64 {
    return a + b;
}

test "func_ptr_inferred" {
    var fp = add;
    var result = fp(1, 2);
    assert(result == 3);
}
