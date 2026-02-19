// Expected: PASS: generic_func_basic

func identity<T>(x: T) -> T {
    return x;
}

test "generic_func_basic" {
    var a = identity(42);
    var b = identity("hello");
    var c = identity(true);

    assert(a == 42);
    assert(b == "hello");
    assert(c == true);
}
