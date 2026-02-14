// Expected: PASS: vec_eq
// Test Vec == and != operators

struct Point { x: i64, y: i64 }

impl Eq for Point {
    func eq(other: Point): bool {
        return self.x == other.x && self.y == other.y;
    }
}

test "vec_eq" {
    // Equal i64 vecs
    var a = new Vec<i64>{1, 2, 3};
    var b = new Vec<i64>{1, 2, 3};
    assert(a == b);

    // Unequal elements
    var c = new Vec<i64>{1, 2, 4};
    assert(a != c);

    // Different lengths
    var d = new Vec<i64>{1, 2};
    assert(a != d);

    // Empty vecs
    var e = new Vec<i64>{};
    var f = new Vec<i64>{};
    assert(e == f);
    assert(e != a);

    // String vecs
    var s1 = new Vec<string>{"hello", "world"};
    var s2 = new Vec<string>{"hello", "world"};
    var s3 = new Vec<string>{"hello", "whist"};
    assert(s1 == s2);
    assert(s1 != s3);

    // Struct vecs (with Eq impl)
    var p1 = new Vec<Point>{new Point{x: 1, y: 2}, new Point{x: 3, y: 4}};
    var p2 = new Vec<Point>{new Point{x: 1, y: 2}, new Point{x: 3, y: 4}};
    var p3 = new Vec<Point>{new Point{x: 1, y: 2}, new Point{x: 5, y: 6}};
    assert(p1 == p2);
    assert(p1 != p3);

    // != operator
    assert(!(a != b));
    assert(!(a == c));
}
