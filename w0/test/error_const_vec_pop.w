// ERROR TEST: Cannot pop from const Vec
// Expected error: Cannot call mutating method 'pop' on const 'v'

func main(): i32 {
    const v = new Vec<i64>{};
    v.pop();
    return 0;
}
