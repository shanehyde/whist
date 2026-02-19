// ERROR TEST: Cannot insert into const Vec
// Expected error: Cannot call mutating method 'insert' on const 'v'

func main() -> i32 {
    const v = new Vec<i64>{1, 2, 3};
    v.insert(1, 99);
    return 0;
}
