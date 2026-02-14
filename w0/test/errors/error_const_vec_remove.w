// ERROR TEST: Cannot remove from const Vec
// Expected error: Cannot call mutating method 'remove' on const 'v'

func main(): i32 {
    const v = new Vec<i64>{1, 2, 3};
    v.remove(0);
    return 0;
}
