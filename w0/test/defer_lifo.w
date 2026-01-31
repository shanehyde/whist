// Test that defers execute in LIFO order
var order: i64 = 0;
var result: i64 = 0;

func record(n: i64): void {
    // Build a number by shifting in digits
    // If called in order 1, 2, 3 -> result = 123
    result = result * 10 + n;
}

func test_lifo(): void {
    defer record(3);  // Should execute last
    defer record(2);  // Should execute second
    defer record(1);  // Should execute first
}

func main(): i32 {
    result = 0;
    test_lifo();
    // If LIFO order is correct: 1 executes first, then 2, then 3
    // result = ((0 * 10 + 1) * 10 + 2) * 10 + 3 = 123
    if (result != 123) {
        return 1;
    }
    return 0;
}
