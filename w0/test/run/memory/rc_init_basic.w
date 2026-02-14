// Expected: PASS: rc_init_basic

struct Resource {
    id: i64,
}

trait Drop {
    func drop(): void;
}

impl Drop for Resource {
    func drop(): void {}
}

impl Resource {
    func init(id: i64) {
        self.id = id;
    }
}

test "rc_init_basic" {
    var r = new Resource(42);
    assert(r.id == 42);
}
