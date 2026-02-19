// ERROR TEST: Cannot call mutable method on const struct
// Expected error: Cannot call mutating method 'move' on const 'p'

struct Point {
    x: i64,
    y: i64,
}

func (Point) move(dx: i64, dy: i64) -> void {
    self.x = self.x + dx;
    self.y = self.y + dy;
}

func main() -> i32 {
    const p = new Point {x: 10, y: 20};
    p.move(5, 5);
    return 0;
}
