// Test basic RC allocation with new

struct Point {
    x: i64,
    y: i64,
}

func main(): i32 {
    var p = new Point { x: 10, y: 20 };
    if (p.x != 10) {
        return 1;
    }
    if (p.y != 20) {
        return 2;
    }
    return 0;
}
