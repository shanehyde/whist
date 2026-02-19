// Type-check only: closures that capture outer variables

func apply(f: func(i64) -> i64, x: i64) -> i64 {
    return f(x);
}

func make_adder(n: i64) -> func(i64) -> i64 {
    return |x: i64| -> i64 x + n;
}

func test_basic_capture() -> void {
    var offset: i64 = 10;
    var f = |x: i64| -> i64 x + offset;
    var result = apply(f, 5);
}

func test_multi_capture() -> void {
    var a: i64 = 1;
    var b: i64 = 2;
    var f = |x: i64| -> i64 x + a + b;
}

func test_nested_capture() -> void {
    var x: i64 = 42;
    var outer = |a: i64| -> i64 {
        var inner = |b: i64| -> i64 x + a + b;
        return inner(1);
    };
}
