// Expected: PASS: triple_string
test "triple_string" {
    // Basic multi-line with indentation stripping
    var s = """
        hello
        world
        """;
    assert(s == "hello\nworld");

    // Inline (no newlines)
    var one_line = """hello""";
    assert(one_line == "hello");

    // Embedded quotes
    var quotes = """
        say "hello"
        use "" empty
        """;
    assert(quotes == "say \"hello\"\nuse \"\" empty");

    // Escape sequences
    var esc = """
        tab:\there
        newline:\n!
        """;
    assert(esc == "tab:\there\nnewline:\n!");

    // Empty triple-quoted string
    var empty = """""";
    assert(empty == "");

    // Relative indentation preservation
    var nested = """
        if (x) {
            return 1;
        }
        """;
    assert(nested == "if (x) {\n    return 1;\n}");
}
