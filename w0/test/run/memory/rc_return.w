// Test returning RC value from function

struct Point {
    x: i64,
    y: i64,
}

func make_point(x: i64, y: i64): Point {
    var p = new Point { x: x, y: y };
    return p;
}

func main(): i32 {
    var p: Point = make_point(10, 32);
    if (p.x + p.y != 42) {
        return 1;
    }
    return 0;
}
