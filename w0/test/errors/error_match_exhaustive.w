// Expected error: missing variant 'Blue'

enum Color {
    Red,
    Green,
    Blue,
}

func main() -> i32 {
    var c: Color = Color::Red;
    match (c) {
        Red => { return 0; },
        Green => { return 1; },
    }
    return 0;
}
