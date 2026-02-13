func greet(): void {
    return;
}

func add(a: i64, b: i64): i64 {
    return a + b;
}

func main(): i32 {
    var fp = add;
    var result = fp(1, 2);
    return 0;
}
