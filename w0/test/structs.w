// Test struct definitions and member access

struct Point {
    x: int,
    y: int,
}

func distance(p: *Point): int {
    return p->x + p->y;
}

func main(): int {
    var p: Point;
    p.x = 10;
    p.y = 20;
    return distance(&p);
}
