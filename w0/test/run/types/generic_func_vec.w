// Expected: PASS: generic_func_vec

func vec_count<T>(v: Vec<T>) -> i64 {
    return v.count;
}

test "generic_func_vec" {
    var v = new Vec<i64>{1, 2, 3};

    var n = vec_count(v);
    assert(n == 3);
}
