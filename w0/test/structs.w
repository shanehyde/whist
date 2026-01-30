// Test struct definitions and member access

struct Point {
    x: int32,
    y: int32,
}

func distance(p: *Point): int32 {
    return p->x + p->y;
}

func main(): int32 {
    var p: Point;
    p.x = 10;
    p.y = 20;
    return distance(&p);
}
