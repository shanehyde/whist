// Expected error: 'Purple' is not a variant of enum 'Color'

enum Color {
    Red,
    Green,
    Blue,
}

func main(): i32 {
    var c: Color = Color::Red;
    if let Purple = c {
        return 0;
    }
    return 0;
}
