// Expected: PASS: enums
// Test enum definitions

enum Color {
    Red,
    Green,
    Blue,
}

test "enums" {
    var c: Color;
    c = Color::Red;
}
