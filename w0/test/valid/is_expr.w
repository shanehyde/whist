// Standalone is expression (no bindings) as bool expression

enum Option<T> {
    Some(T),
    None,
}

enum Color {
    Red,
    Green,
    Blue,
}

func main() -> i32 {
    var opt: Option<i64> = Option::Some(42);
    var b: bool = opt is Some;
    var c: bool = opt is Option::None;

    var color: Color = Color::Red;
    var d: bool = color is Red;
    var e: bool = color is Color::Green;

    if (opt is Some) {
        return 0;
    }
    return 1;
}
