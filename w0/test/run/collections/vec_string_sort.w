// Expected: PASS: vec_string_sort
// Test Vec<string> sort()

test "vec_string_sort" {
    var v = new Vec<string>{"cherry", "apple", "banana"};
    v.sort();
    assert(v[0] == "apple");
    assert(v[1] == "banana");
    assert(v[2] == "cherry");

    // Empty vec
    var empty = new Vec<string>{};
    empty.sort();
    assert(empty.count == 0);

    // Single element
    var single = new Vec<string>{"only"};
    single.sort();
    assert(single[0] == "only");

    // Already sorted
    var sorted = new Vec<string>{"a", "b", "c"};
    sorted.sort();
    assert(sorted[0] == "a" && sorted[1] == "b" && sorted[2] == "c");

    // Reverse order
    var rev = new Vec<string>{"z", "m", "a"};
    rev.sort();
    assert(rev[0] == "a" && rev[1] == "m" && rev[2] == "z");
}
