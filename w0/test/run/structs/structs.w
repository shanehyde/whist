// Expected: PASS: structs
// Test struct definitions and member access

struct Point {
    x: i32,
    y: i32,
}

func distance(p: Point): i32 {
    return p.x + p.y;
}

test "structs" {
    var p = new Point {x: 10, y: 30};
    assert(distance(p) == 40);
}
