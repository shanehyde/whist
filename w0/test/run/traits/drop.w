// Expected: PASS: drop_basic
// Expected: PASS: drop_generic
// Expected: PASS: drop_generic_impl

trait Drop {
    func drop(): void;
}

struct Resource {
    id: i64,
}

impl Drop for Resource {
    func drop(): void {
        // cleanup
    }
}

struct Inner {
    value: i64,
}

struct Box<T> {
    item: T,
}

impl Drop for Box<T> {
    func drop(): void {
        // cleanup
    }
}

test "drop_basic" {
    var r = new Resource { id: 1 };
}

// Test: generic struct with RC fields gets proper nested cleanup
test "drop_generic" {
    var inner = new Inner { value: 42 };
    var b = new Box<Inner> { item: inner };
}

// Test: impl Drop for a generic struct type-checks
test "drop_generic_impl" {
    var b = new Box<i64> { item: 42 };
}
