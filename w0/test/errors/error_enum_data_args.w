// Expected error: expects 1 argument(s), got 2

enum Shape {
    Circle(f64),
    None,
}

func main() -> i32 {
    var s = Shape::Circle(1.0, 2.0);
    return 0;
}
