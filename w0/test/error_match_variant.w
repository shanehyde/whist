// Expected error: is not a variant of enum

enum Color {
    Red,
    Green,
    Blue,
}

func main(): i32 {
    var c: Color = Color::Red;
    match (c) {
        Red => { return 0; },
        Purple => { return 1; },
    }
    return 0;
}
