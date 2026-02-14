// Expected: PASS: const_struct_const_method
// Test that const methods can be called on const struct bindings

struct Point {
    x: i64,
    y: i64,
}

func (const Point) sum(): i64 {
    return self.x + self.y;
}

test "const_struct_const_method" {
    const p = new Point {x: 10, y: 20};
    var s = p.sum();
}
