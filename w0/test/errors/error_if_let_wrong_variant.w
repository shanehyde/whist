// Expected error: 'Purple' is not a variant of enum 'Color'

enum Color {
    Red,
    Green,
    Blue,
}

func main() -> i32 {
    var c: Color = Color::Red;
    if (c is Purple) {
        return 0;
    }
    return 0;
}
