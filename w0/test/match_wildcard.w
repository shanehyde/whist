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

func main(): i32 {
    var c: Color = Color::Green;

    var result: i64 = 0;
    match (c) {
        Red => { result = 1; },
        _ => { result = 99; },
    }

    if (result != 99) {
        return 1;
    }

    // Test wildcard with data enum
    var s: Shape = Shape::Rect(3.0, 4.0);
    match (s) {
        Circle(r) => { result = 1; },
        _ => { result = 42; },
    }

    if (result != 42) {
        return 2;
    }

    return 0;
}
