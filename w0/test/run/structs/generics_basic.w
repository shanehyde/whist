// Expected: PASS: generics_basic
// Basic generic struct test

struct Box<T> {
    value: T,
}

test "generics_basic" {
    var b = new Box<i64> {value: 42};
    assert(b.value == 42);
}
