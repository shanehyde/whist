// Expected: PASS: eq_runtime
// Test value equality via Eq trait and sameref

struct Point {
    x: i64,
    y: i64,
}

trait Eq {
    func eq(other: Self): bool;
}

impl Eq for Point {
    func eq(other: Point): bool {
        return self.x == other.x && self.y == other.y;
    }
}

enum Shape {
    Circle(i64),
    Rect(i64, i64),
    None,
}

test "eq_runtime" {
    // Struct equality
    var a = new Point { x: 1, y: 2 };
    var b = new Point { x: 1, y: 2 };
    var c = new Point { x: 3, y: 4 };

    assert(a == b);
    assert(!(a != b));
    assert(!(a == c));
    assert(a != c);

    // sameref: a and b are different allocations
    assert(!sameref(a, b));
    var d = a;
    assert(sameref(a, d));

    // Data enum equality
    var s1 = Shape::Circle(5);
    var s2 = Shape::Circle(5);
    var s3 = Shape::Rect(1, 2);
    var s4 = Shape::None;
    var s5 = Shape::None;

    assert(s1 == s2);
    assert(!(s1 == s3));
    assert(!(s1 == s4));
    assert(s4 == s5);
}
