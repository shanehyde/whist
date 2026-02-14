// Expected: PASS: drop_basic

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

test "drop_basic" {
    var r = new Resource { id: 1 };
}
