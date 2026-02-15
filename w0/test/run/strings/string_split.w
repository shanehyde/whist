// Expected: PASS: string_split_basic
// Expected: PASS: string_split_empty
// Expected: PASS: string_split_no_match
// Expected: PASS: string_split_consecutive
// Expected: PASS: string_split_edges

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
