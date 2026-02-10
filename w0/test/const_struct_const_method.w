// Test that const methods can be called on const struct bindings

struct Point {
    x: i64,
    y: i64,
}

func (const Point) sum(): i64 {
    return self.x + self.y;
}

func main(): i32 {
    const p = new Point {x: 10, y: 20};
    var s = p.sum();
    return 0;
}
