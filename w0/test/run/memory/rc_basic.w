// Expected: PASS: rc_basic
// Test basic RC allocation with new

struct Point {
    x: i64,
    y: i64,
}

test "rc_basic" {
    var p = new Point { x: 10, y: 20 };
    assert(p.x == 10);
    assert(p.y == 20);
}
