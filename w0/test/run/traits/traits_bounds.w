// Expected: PASS: traits_bounds

trait HasValue {
    func value(): i64;
}

struct Wrapper {
    v: i64,
}

impl HasValue for Wrapper {
    func value(): i64 {
        return self.v;
    }
}

struct Box<T: HasValue> {
    item: T,
}

test "traits_bounds" {
    var w = new Wrapper {v: 42};
    var b = new Box<Wrapper> {item: w};
    assert(w.value() == 42);
}
