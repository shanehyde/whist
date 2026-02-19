// Expected: PASS: generic_func_multi_param

func first<A, B>(a: A, b: B) -> A {
    return a;
}

func second<A, B>(a: A, b: B) -> B {
    return b;
}

test "generic_func_multi_param" {
    var a = first(10, "hello");
    var b = second(10, "world");

    assert(a == 10);
    assert(b == "world");
}
