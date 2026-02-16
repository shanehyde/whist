// Lambda syntax: assignment, type-checking, function arguments

func apply(f: func(i64): i64, x: i64): i64 {
    return f(x);
}

func apply_void(f: func(i64): void, x: i64): void {
    f(x);
}

func main(): i32 {
    // Expression body
    var square: func(i64): i64 = |x: i64| x * x;

    // Block body with return type
    var add: func(i64, i64): i64 = |a: i64, b: i64| -> i64 { return a + b; };

    // Block body, void return
    var noop: func(i64): void = |x: i64| { };

    // Empty params
    var say_hi: func(): i64 = || 42;

    // Lambda passed as function argument
    var result = apply(|x: i64| x + 1, 42);

    return 0;
}
