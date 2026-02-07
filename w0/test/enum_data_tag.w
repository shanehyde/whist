// Test .tag member access on data enums

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

func main(): i32 {
    var s = Shape::Circle(3.14);
    var tag: i32 = s.tag;
    return 0;
}
