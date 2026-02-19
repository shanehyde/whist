// ERROR TEST: Cannot push to const Vec
// Expected error: Cannot call mutating method 'push' on const 'v'

func main() -> i32 {
    const v = new Vec<i64>{};
    v.push(42);
    return 0;
}
