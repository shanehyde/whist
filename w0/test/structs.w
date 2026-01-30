// Test struct definitions and member access

struct Point {
    x: i32,
    y: i32,
}

func distance(p: *Point): i32 {
    return p->x + p->y;
}

func main(): i32 {
    var p: Point = {x: 10, y: 30};
    // p.x = 10;
    // p.y = 20;
    return distance(&p);
}
