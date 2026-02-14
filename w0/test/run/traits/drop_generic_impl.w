// Expected: PASS: drop_generic_impl
// Test: impl Drop for a generic struct type-checks

trait Drop {
    func drop(): void;
}

struct Box<T> {
    item: T,
}

impl Drop for Box<T> {
    func drop(): void {
        // cleanup
    }
}

test "drop_generic_impl" {
    var b = new Box<i64> { item: 42 };
}
