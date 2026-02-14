// Expected: PASS: defer

func cleanup(): void {
}

func test_single_defer(): i64 {
    defer cleanup();
    return 42;
}

func test_multiple_defers(): i64 {
    defer cleanup();
    defer cleanup();
    defer cleanup();
    return 100;
}

func test_void_defer(): void {
    defer cleanup();
}

test "defer" {
    var result = test_single_defer();
    assert(result == 42);

    var result2 = test_multiple_defers();
    assert(result2 == 100);

    test_void_defer();
}
