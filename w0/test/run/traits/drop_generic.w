// Expected: PASS: drop_generic
// Test: generic struct with RC fields gets proper nested cleanup

struct Inner {
    value: i64,
}

struct Box<T> {
    item: T,
}

test "drop_generic" {
    var inner = new Inner { value: 42 };
    var b = new Box<Inner> { item: inner };
}
