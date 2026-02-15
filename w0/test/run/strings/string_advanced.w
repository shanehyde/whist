// Expected: PASS: triple_string
// Expected: PASS: string_split_basic
// Expected: PASS: string_split_empty
// Expected: PASS: string_split_no_match
// Expected: PASS: string_split_consecutive
// Expected: PASS: string_split_edges
// Expected: PASS: string_index_of

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

test "string_split_basic" {
    var parts = "a,b,c".split(",");
    assert(parts.count == 3);
    assert(parts[0] == "a");
    assert(parts[1] == "b");
    assert(parts[2] == "c");
}

test "string_split_empty" {
    var parts = "".split(",");
    assert(parts.count == 1);
    assert(parts[0] == "");
}

test "string_split_no_match" {
    var parts = "hello".split(",");
    assert(parts.count == 1);
    assert(parts[0] == "hello");
}

test "string_split_consecutive" {
    var parts = "a,,b".split(",");
    assert(parts.count == 3);
    assert(parts[0] == "a");
    assert(parts[1] == "");
    assert(parts[2] == "b");
}

test "string_split_edges" {
    var parts = ",a,".split(",");
    assert(parts.count == 3);
    assert(parts[0] == "");
    assert(parts[1] == "a");
    assert(parts[2] == "");
}

test "string_index_of" {
    assert("hello world".index_of("world") == 6);
    assert("hello world".index_of("hello") == 0);
    assert("hello world".index_of("xyz") == -1);
    assert("abcabc".index_of("bc") == 1);
    assert("hello".index_of("") == 0);
}
