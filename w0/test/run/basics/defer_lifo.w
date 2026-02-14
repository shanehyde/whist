// Expected: PASS: defer_lifo
// Test that defers execute in LIFO order

func record(n: i64): void {
}

func test_lifo(): void {
    defer record(3);  // Should execute last
    defer record(2);  // Should execute second
    defer record(1);  // Should execute first
}

test "defer_lifo" {
    test_lifo();
}
