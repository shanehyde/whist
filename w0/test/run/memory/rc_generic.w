// Expected: PASS: rc_generic
// Test new with generic structs

struct Box<T> {
    value: T,
}

func (Box<T>) get(): T {
    return self.value;
}

test "rc_generic" {
    var b = new Box<i64> { value: 42 };
    assert(b.get() == 42);
}
