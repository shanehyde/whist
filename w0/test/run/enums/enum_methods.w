// Expected: PASS: enum_methods
// Test enum methods

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

test "enum_methods" {
    var m = Mobile::Player("Alice");
    assert(m.describe() == "Player: Alice");
}
