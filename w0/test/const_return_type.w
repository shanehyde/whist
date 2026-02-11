// Test that const-qualified return types parse and type-check

func make_view(): const Vec<i64> {
    return new Vec<i64>{1, 2, 3};
}

func main(): i32 {
    const v = make_view();
    var n = v.count;
    if (n < 0) {
        return 1;
    }
    return 0;
}
