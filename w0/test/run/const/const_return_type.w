// Expected: PASS: const_return_type
// Test that const-qualified return types parse and type-check

func make_view(): const Vec<i64> {
    return new Vec<i64>{1, 2, 3};
}

test "const_return_type" {
    const v = make_view();
    var n = v.count;
    assert(n >= 0);
}
