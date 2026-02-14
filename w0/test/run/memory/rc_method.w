// Test calling methods on RC-allocated structs

struct Point {
    x: i64,
    y: i64,
}

func (Point) sum(): i64 {
    return self.x + self.y;
}

func (Point) move(dx: i64, dy: i64): void {
    self.x = self.x + dx;
    self.y = self.y + dy;
}

func main(): i32 {
    var p = new Point { x: 10, y: 20 };
    p.move(5, 7);
    if (p.sum() != 42) {
        return 1;
    }
    return 0;
}
