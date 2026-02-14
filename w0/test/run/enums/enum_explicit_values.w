// Test explicit integer values for enum variants and enum->integer casts

enum TokenType {
    Eof = -1,
    Plus = 43,
    Minus,
    Star = 42,
    Slash,
}

func main(): i32 {
    var eof: i64 = TokenType::Eof as i64;
    if (eof != -1) {
        return 1;
    }

    var plus: i64 = TokenType::Plus as i64;
    if (plus != 43) {
        return 2;
    }

    var minus: i64 = TokenType::Minus as i64;
    if (minus != 44) {
        return 3;
    }

    var star: i64 = TokenType::Star as i64;
    if (star != 42) {
        return 4;
    }

    var slash: i64 = TokenType::Slash as i64;
    if (slash != 43) {
        return 5;
    }

    return 0;
}
