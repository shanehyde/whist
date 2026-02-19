// Expected: PASS: rc_drop_basic
// RC RUNTIME TEST: Drop method is called when refcount reaches 0

trait Drop {
    func drop() -> void;
}

struct Resource {
    id: i64,
}

impl Drop for Resource {
    func drop() -> void {
        // Drop method called - resource cleaned up
    }
}

test "rc_drop_basic" {
    var r = new Resource { id: 1 };
}
