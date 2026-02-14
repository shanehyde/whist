// Expected: PASS: eq_basic

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

test "eq_basic" {
    var a = new Point { x: 1, y: 2 };
    var b = new Point { x: 1, y: 2 };
    var c = new Point { x: 3, y: 4 };

    assert(a == b);
    assert(a != c);
}
