// Expected: PASS: const_binding_field_const_method
// Valid: const methods remain callable through fields of const bindings

struct Point {
    x: i64,
    y: i64,
}

struct Wrap {
    p: Point,
}

func (const Point) sum(): i64 {
    return self.x + self.y;
}

test "const_binding_field_const_method" {
    const w = new Wrap { p: new Point { x: 10, y: 20 } };
    var s = w.p.sum();
    var x = w.p.x;
}
