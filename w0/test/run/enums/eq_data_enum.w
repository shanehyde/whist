// Expected: PASS: eq_data_enum
// Test equality on data enums

enum Shape {
    Circle(i64),
    Rect(i64, i64),
    None,
}

test "eq_data_enum" {
    var a = Shape::Circle(5);
    var b = Shape::Circle(5);
    var c = Shape::Rect(1, 2);

    if (a == b) {}
    if (a != c) {}
}
