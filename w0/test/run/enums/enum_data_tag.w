// Expected: PASS: enum_data_tag
// Test .tag member access on data enums

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

test "enum_data_tag" {
    var s = Shape::Circle(3.14);
    var tag: i32 = s.tag;
}
