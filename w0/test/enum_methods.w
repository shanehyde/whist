import std;
use std.println;

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

func main() : i32 {
    var m = Mobile::Player("Alice");
    println($"{m.describe()}");
    return 0;
}