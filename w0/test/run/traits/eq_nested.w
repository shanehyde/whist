// Expected: PASS: eq_nested

struct Inner {
    val: i64,
}

struct Outer {
    inner: Inner,
    tag: i64,
}

trait Eq {
    func eq(other: Self): bool;
}

impl Eq for Inner {
    func eq(other: Inner): bool {
        return self.val == other.val;
    }
}

impl Eq for Outer {
    func eq(other: Outer): bool {
        return self.inner == other.inner && self.tag == other.tag;
    }
}

test "eq_nested" {
    var a = new Outer { inner: new Inner { val: 1 }, tag: 10 };
    var b = new Outer { inner: new Inner { val: 1 }, tag: 10 };

    assert(a == b);
}
