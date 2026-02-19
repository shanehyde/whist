// ERROR TEST: Cannot swap_remove from const Vec
// Expected error: Cannot call mutating method 'swap_remove' on const 'v'

func main() -> i32 {
    const v = new Vec<i64>{1, 2, 3};
    v.swap_remove(0);
    return 0;
}
