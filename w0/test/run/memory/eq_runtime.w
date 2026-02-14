// Test value equality via Eq trait and sameref

import std;

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

func main(): i32 {
    // Struct equality
    var a = new Point { x: 1, y: 2 };
    var b = new Point { x: 1, y: 2 };
    var c = new Point { x: 3, y: 4 };

    if (!(a == b)) { return 1; }
    if (a != b)    { return 2; }
    if (a == c)    { return 3; }
    if (!(a != c)) { return 4; }

    // sameref: a and b are different allocations
    if (sameref(a, b))  { return 5; }
    var d = a;
    if (!sameref(a, d)) { return 6; }

    // Data enum equality
    var s1 = Shape::Circle(5);
    var s2 = Shape::Circle(5);
    var s3 = Shape::Rect(1, 2);
    var s4 = Shape::None;
    var s5 = Shape::None;

    if (!(s1 == s2)) { return 7; }
    if (s1 == s3)    { return 8; }
    if (s1 == s4)    { return 9; }
    if (!(s4 == s5)) { return 10; }

    return 0;
}
