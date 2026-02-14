func apply(f: func(i64): i64, x: i64): i64 {
    return f(x);
}

func twice(x: i64): i64 {
    return x * 2;
}

func main(): i32 {
    var result = apply(twice, 5);
    return 0;
}
