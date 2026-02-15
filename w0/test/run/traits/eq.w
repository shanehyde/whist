// Expected: PASS: eq_basic
// Expected: PASS: eq_generic
// Expected: PASS: eq_nested
// Expected: PASS: eq_sameref

trait Eq {
    func eq(other: Self): bool;
}

struct Point {
    x: i64,
    y: i64,
}

impl Eq for Point {
    func eq(other: Point): bool {
        return self.x == other.x && self.y == other.y;
    }
}

struct Box<T> {
    value: T,
}

impl Eq for Box<T> {
    func eq(other: Box<T>): bool {
        return self.value == other.value;
    }
}

struct Inner {
    val: i64,
}

struct Outer {
    inner: Inner,
    tag: i64,
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

struct Obj {
    val: i64,
}

test "eq_basic" {
    var a = new Point { x: 1, y: 2 };
    var b = new Point { x: 1, y: 2 };
    var c = new Point { x: 3, y: 4 };

    assert(a == b);
    assert(a != c);
}

test "eq_generic" {
    var a = new Box<i64> { value: 42 };
    var b = new Box<i64> { value: 42 };

    assert(a == b);
}

test "eq_nested" {
    var a = new Outer { inner: new Inner { val: 1 }, tag: 10 };
    var b = new Outer { inner: new Inner { val: 1 }, tag: 10 };

    assert(a == b);
}

test "eq_sameref" {
    var a = new Obj { val: 1 };
    var b = a;
    var c = new Obj { val: 1 };

    assert(sameref(a, b));
    assert(!sameref(a, c));
}
