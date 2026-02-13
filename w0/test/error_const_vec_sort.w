// ERROR TEST: Cannot sort const Vec
// Expected error: Cannot call mutating method 'sort' on const 'v'

func main(): i32 {
    const v = new Vec<i64>{3, 1, 2};
    v.sort();
    return 0;
}
