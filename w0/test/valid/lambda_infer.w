// Lambda parameter type inference

func apply(f: func(i64) -> i64, x: i64) -> i64 {
    return f(x);
}

func main() -> i32 {
    // Infer from regular function param type
    var result = apply(|x| x + 1, 42);

    // Mixed: some typed, some inferred
    // (single param inferred)
    var r2 = apply(|x| x * 2, 10);

    return 0;
}
