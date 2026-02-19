// ERROR TEST: Cannot write to index of const Vec
// Expected error: Cannot write to index of const 'v'

func main() -> i32 {
    const v = new Vec<i64>{1, 2, 3};
    v[0] = 99;
    return 0;
}
