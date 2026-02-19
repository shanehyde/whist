// Test hex, octal, and \e escape sequences in strings
// Expected exit: 0

func main() -> i32 {
    // Hex escape in string
    var hex_str: string = "\x41\x42\x43";
    if (hex_str != "ABC") {
        return 1;
    }

    // Octal escape in string
    var octal_str: string = "\101\102\103";
    if (octal_str != "ABC") {
        return 2;
    }

    // \e escape (ESC = 0x1b)
    var esc_str: string = "\e[31m";
    var hex_esc_str: string = "\x1b[31m";
    if (esc_str != hex_esc_str) {
        return 3;
    }

    // Mixed escapes
    var mixed: string = "\x48ello\n";
    if (mixed != "Hello\n") {
        return 4;
    }

    return 0;
}
