// Expected error: Variant 'Circle' expects 1 binding(s), got 2

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

func main() -> i32 {
    var s: Shape = Shape::Circle(5.0);
    if (s is Circle(a, b)) {
        return 0;
    }
    return 0;
}
