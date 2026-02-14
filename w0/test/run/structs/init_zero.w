// Expected: PASS: init_zero

struct Counter {
    count: i64,
    label: i64,
}

impl Counter {
    func init(c: i64) {
        self.count = c;
    }
}

test "init_zero" {
    var c = new Counter(42);
    assert(c.count == 42);
    assert(c.label == 0);
}
