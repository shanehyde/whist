// Expected: PASS: enum_explicit_values
// Test explicit integer values for enum variants and enum->integer casts

enum TokenType {
    Eof = -1,
    Plus = 43,
    Minus,
    Star = 42,
    Slash,
}

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
