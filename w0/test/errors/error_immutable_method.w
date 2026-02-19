// Immutable method
// Expected error: Cannot modify field 'x' through const 'self'

struct Point {
    x: i64,
    y: i64,
}

// Immutable method
func (const Point) move(dx: i64, dy: i64) -> void {
    self.x = self.x + dx;
    self.y = self.y + dy;
}

func main() -> i64 {
    var p = new Point {x: 10, y: 20};
    p.move(5, 5);

    return 0;
}
