// ERROR TEST: Cannot shrink_to_fit on const Vec
// Expected error: Cannot call mutating method 'shrink_to_fit' on const 'v'

func main(): i32 {
    const v = new Vec<i64>{1, 2, 3};
    v.shrink_to_fit();
    return 0;
}
