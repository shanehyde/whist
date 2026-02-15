// Expected: PASS: enums
// Expected: PASS: enum_explicit_values
// Expected: PASS: enum_data
// Expected: PASS: enum_data_tag
// Expected: PASS: enum_methods

enum Color {
    Red,
    Green,
    Blue,
}

enum TokenType {
    Eof = -1,
    Plus = 43,
    Minus,
    Star = 42,
    Slash,
}

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

enum Mobile {
    Player(string),
    Enemy(i32),
}

func (Mobile) describe(): string {
    match (self) {
        Mobile::Player(name) => return $"Player: {name}";
        Mobile::Enemy(level) => return $"Enemy of level {level}";
    }
}

// --- enums ---

test "enums" {
    var c: Color;
    c = Color::Red;
}

// --- enum_explicit_values ---

test "enum_explicit_values" {
    var eof: i64 = TokenType::Eof as i64;
    assert(eof == -1);

    var plus: i64 = TokenType::Plus as i64;
    assert(plus == 43);

    var minus: i64 = TokenType::Minus as i64;
    assert(minus == 44);

    var star: i64 = TokenType::Star as i64;
    assert(star == 42);

    var slash: i64 = TokenType::Slash as i64;
    assert(slash == 43);
}

// --- enum_data ---

test "enum_data" {
    var s: Shape = Shape::Circle(3.14);
    var r: Shape = Shape::Rect(10.0, 20.0);
    var n: Shape = Shape::None;

    // Type inference
    var s2 = Shape::Circle(2.0);
    var r2 = Shape::Rect(1.0, 2.0);
    var n2 = Shape::None;
}

// --- enum_data_tag ---

test "enum_data_tag" {
    var s = Shape::Circle(3.14);
    var tag: i32 = s.tag;
}

// --- enum_methods ---

test "enum_methods" {
    var m = Mobile::Player("Alice");
    assert(m.describe() == "Player: Alice");
}
