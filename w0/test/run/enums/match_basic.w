// Test basic match on data enum

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

func main(): i32 {
    var s: Shape = Shape::Circle(3.14);

    var result: i64 = 0;
    match (s) {
        Circle(r) => {
            // r should be 3.14
            if (r > 3.0) {
                result = 1;
            }
        },
        Rect(w, h) => {
            result = 2;
        },
        None => {
            result = 3;
        },
    }

    // result should be 1
    if (result != 1) {
        return 1;
    }

    // Test matching Rect
    var r2: Shape = Shape::Rect(10.0, 20.0);
    var area: f64 = 0.0;
    match (r2) {
        Circle(r) => { area = 0.0; },
        Rect(w, h) => { area = w * h; },
        None => { area = 0.0; },
    }

    if (area != 200.0) {
        return 2;
    }

    // Test matching None
    var n: Shape = Shape::None;
    var tag: i64 = 0;
    match (n) {
        Circle(r) => { tag = 1; },
        Rect(w, h) => { tag = 2; },
        None => { tag = 3; },
    }

    if (tag != 3) {
        return 3;
    }

    return 0;
}
