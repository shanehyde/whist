// ERROR TEST: Cannot reserve on const Vec
// Expected error: Cannot call mutating method 'reserve' on const 'v'

func main() -> i32 {
    const v = new Vec<i64>{};
    v.reserve(1);
    return 0;
}
