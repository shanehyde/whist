// Expected: PASS: match_wildcard
// Test match with wildcard arm

enum Color {
    Red,
    Green,
    Blue,
    Yellow,
}

enum Shape {
    Circle(f64),
    Rect(f64, f64),
}

test "match_wildcard" {
    var c: Color = Color::Green;

    var result: i64 = 0;
    match (c) {
        Red => { result = 1; },
        _ => { result = 99; },
    }

    assert(result == 99);

    // Test wildcard with data enum
    var s: Shape = Shape::Rect(3.0, 4.0);
    match (s) {
        Circle(r) => { result = 1; },
        _ => { result = 42; },
    }

    assert(result == 42);
}
