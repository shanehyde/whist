// Expected: PASS: rc_return
// Test returning RC value from function

struct Point {
    x: i64,
    y: i64,
}

func make_point(x: i64, y: i64): Point {
    var p = new Point { x: x, y: y };
    return p;
}

test "rc_return" {
    var p: Point = make_point(10, 32);
    assert(p.x + p.y == 42);
}
