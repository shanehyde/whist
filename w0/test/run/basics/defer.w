// Expected: PASS: defer
// Expected: PASS: defer_lifo

func cleanup(): void {
}

func record(n: i64): void {
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

// Test that defers execute in LIFO order
func test_lifo(): void {
    defer record(3);  // Should execute last
    defer record(2);  // Should execute second
    defer record(1);  // Should execute first
}

test "defer" {
    var result = test_single_defer();
    assert(result == 42);

    var result2 = test_multiple_defers();
    assert(result2 == 100);

    test_void_defer();
}

test "defer_lifo" {
    test_lifo();
}
