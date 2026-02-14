// Expected error: expected 'f64', got 'string'

enum Shape {
    Circle(f64),
    None,
}

func main(): i32 {
    var s = Shape::Circle("hello");
    return 0;
}
