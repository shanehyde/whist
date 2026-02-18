// Test character escape sequences
// Expected exit: 0

func main(): i32 {
    // Simple escapes
    var newline: char = '\n';
    var tab: char = '\t';
    var carriage: char = '\r';
    var backslash: char = '\\';
    var single_quote: char = '\'';
    var double_quote: char = '\"';
    var null_char: char = '\0';
    var esc: char = '\e';

    // Hex escapes
    var hex_A: char = '\x41';      // 'A'
    var hex_newline: char = '\x0A'; // newline

    // Octal escapes
    var octal_A: char = '\101';    // 'A' (65 in octal = 101)
    var octal_zero: char = '\0';

    // Regular character
    var regular: char = 'X';

    // Verify hex escape produces correct value
    if (hex_A != 'A') {
        return 1;
    }
    // Verify octal escape produces correct value
    if (octal_A != 'A') {
        return 2;
    }
    // Verify \e produces ESC (0x1b = 27)
    if (esc != '\x1b') {
        return 3;
    }
    // Verify hex newline matches simple escape
    if (hex_newline != '\n') {
        return 4;
    }
    return 0;
}
