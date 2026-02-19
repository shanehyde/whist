// Expected error: Enum tag is read-only; cannot assign to 'Shape.tag'

enum Shape {
    Circle(f64),
    None,
}

func main() -> i32 {
    var s = Shape::Circle(1.0);
    s.tag = 1;
    return 0;
}
