struct Point {
    x: i64,
    y: i64,
}

// Mutable method
func (Point) move(dx: i64, dy: i64): void {
    self.x = self.x + dx;
    self.y = self.y + dy;
}

// Immutable method
func (const Point) sum(): i64 {
    return self.x + self.y;
}

// Test passing struct reference to function
func move_ref(p: Point): void {
    p.move(1, 1);
}

func main(): i32 {
    var p: Point = {x: 10, y: 20};
    p.move(5, 5);
    move_ref(p);     // Calls p.move(1, 1)
    return p.sum();  // Should return 42
}
