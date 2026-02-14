// Test block syntax is valid and passes type-checking.

func add(a: i64, b: i64): i64 {
    return a + b;
}

test "basic arithmetic" {
    var x = add(1, 2);
    assert(x == 3);
}

test "boolean logic" {
    assert(true);
    assert(!false);
}

func main(): i32 {
    return 0;
}
