func add(a: i64, b: i64): i64 {
    return a + b;
}

func main(): i32 {
    var fp: func(i64, i64): i64 = add;
    var result = fp(2, 3);
    return 0;
}
