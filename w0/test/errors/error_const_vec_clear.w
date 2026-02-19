// ERROR TEST: Cannot clear const Vec
// Expected error: Cannot call mutating method 'clear' on const 'v'

func main() -> i32 {
    const v = new Vec<i64>{};
    v.clear();
    return 0;
}
