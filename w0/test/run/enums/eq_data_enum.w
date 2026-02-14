enum Shape {
    Circle(i64),
    Rect(i64, i64),
    None,
}

func main(): i32 {
    var a = Shape::Circle(5);
    var b = Shape::Circle(5);
    var c = Shape::Rect(1, 2);

    if (a == b) {}
    if (a != c) {}

    return 0;
}
