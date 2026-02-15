// Expected: PASS: match_basic
// Expected: PASS: match_expr
// Expected: PASS: match_simple_enum
// Expected: PASS: match_wildcard
// Expected: PASS: match_generic

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

enum Direction {
    North,
    South,
    East,
    West,
}

enum Color {
    Red,
    Green,
    Blue,
    Yellow,
}

enum Animal {
    Dog,
    Cat,
    Other,
}

enum Option<T> {
    Some(T),
    None,
}

// --- match_basic ---

test "match_basic" {
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
    assert(result == 1);

    // Test matching Rect
    var r2: Shape = Shape::Rect(10.0, 20.0);
    var area: f64 = 0.0;
    match (r2) {
        Circle(r) => { area = 0.0; },
        Rect(w, h) => { area = w * h; },
        None => { area = 0.0; },
    }

    assert(area == 200.0);

    // Test matching None
    var n: Shape = Shape::None;
    var tag: i64 = 0;
    match (n) {
        Circle(r) => { tag = 1; },
        Rect(w, h) => { tag = 2; },
        None => { tag = 3; },
    }

    assert(tag == 3);
}

// --- match_expr ---

func describe(a: Animal): string {
    return match (a) {
        Animal::Dog => "canine",
        Animal::Cat => "feline",
        _ => "other",
    };
}

func area_tag(s: Shape): i64 {
    return match (s) {
        Circle(r) => 1,
        Rect(w, h) => 2,
        None => 3,
    };
}

func add_one(v: i64): i64 {
    return v + 1;
}

test "match_expr" {
    // Match expression in variable initialization
    var label = match (Animal::Cat) {
        Dog => "canine",
        Cat => "feline",
        _ => "other",
    };
    assert(label == "feline");

    // Match expression in return context through helper
    var tag = area_tag(Shape::Rect(3.0, 4.0));
    assert(tag == 2);

    // Match expression as a function argument
    var v: i64 = add_one(match (Shape::Circle(9.0)) {
        Circle(r) => 10,
        Rect(w, h) => 20,
        None => 30,
    });
    assert(v == 11);

    assert(describe(Animal::Dog) == "canine");
}

// --- match_simple_enum ---

test "match_simple_enum" {
    var d: Direction = Direction::East;

    var result: i64 = 0;
    match (d) {
        North => { result = 1; },
        South => { result = 2; },
        East => { result = 3; },
        West => { result = 4; },
    }

    assert(result == 3);

    // Test qualified variant names
    var d2: Direction = Direction::West;
    match (d2) {
        Direction::North => { result = 1; },
        Direction::South => { result = 2; },
        Direction::East => { result = 3; },
        Direction::West => { result = 4; },
    }

    assert(result == 4);
}

// --- match_wildcard ---

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

// --- match_generic ---

test "match_generic" {
    var opt: Option<i64> = Option::Some(42);

    var result: i64 = 0;
    match (opt) {
        Some(v) => {
            result = v;
        },
        None => {
            result = -1;
        },
    }

    assert(result == 42);

    // Test None case
    var empty: Option<i64> = Option::None;
    match (empty) {
        Some(v) => { result = v; },
        None => { result = -1; },
    }

    assert(result == -1);
}
