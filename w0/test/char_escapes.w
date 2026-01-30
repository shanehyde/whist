// Test character escape sequences

func main(): i32 {
    // Simple escapes
    var newline: char = '\n';
    var tab: char = '\t';
    var carriage: char = '\r';
    var backslash: char = '\\';
    var single_quote: char = '\'';
    var double_quote: char = '\"';
    var null_char: char = '\0';

    // Hex escapes
    var hex_A: char = '\x41';      // 'A'
    var hex_newline: char = '\x0A'; // newline

    // Octal escapes
    var octal_A: char = '\101';    // 'A' (65 in octal = 101)
    var octal_zero: char = '\0';

    // Regular character
    var regular: char = 'X';

    // Verify hex escape produces correct value
    if (hex_A == 'A') {
        return 0;
    }
    return 1;
}
