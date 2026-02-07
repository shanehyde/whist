// Test data enums (tagged unions)

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

func main(): i32 {
    var s: Shape = Shape::Circle(3.14);
    var r: Shape = Shape::Rect(10.0, 20.0);
    var n: Shape = Shape::None;

    // Type inference
    var s2 = Shape::Circle(2.0);
    var r2 = Shape::Rect(1.0, 2.0);
    var n2 = Shape::None;

    return 0;
}
