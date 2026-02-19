// Expected: PASS: lambda_runtime

func apply(f: func(i64) -> i64, x: i64) -> i64 {
    return f(x);
}

func apply2(f: func(i64, i64) -> i64, a: i64, b: i64) -> i64 {
    return f(a, b);
}

test "lambda_runtime" {
    // Lambda called through variable
    var square: func(i64) -> i64 = |x: i64| x * x;
    assert(square(5) == 25);

    // Lambda passed to higher-order function
    var result = apply(|x: i64| x + 10, 32);
    assert(result == 42);

    // Lambda with multiple params
    var sum = apply2(|a: i64, b: i64| a + b, 20, 22);
    assert(sum == 42);

    // Block body lambda
    var abs_val: func(i64) -> i64 = |x: i64| -> i64 {
        if (x < 0) {
            return -x;
        }
        return x;
    };
    assert(abs_val(-7) == 7);
    assert(abs_val(3) == 3);

    // Empty params lambda
    var get42: func() -> i64 = || 42;
    assert(get42() == 42);

    // Direct call
    var x = (|a: i64| a * 2)(21);
    assert(x == 42);

    // Multiple lambdas in same function
    var inc: func(i64) -> i64 = |x: i64| x + 1;
    var dec: func(i64) -> i64 = |x: i64| x - 1;
    assert(inc(10) == 11);
    assert(dec(10) == 9);
}
