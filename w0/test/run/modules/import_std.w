// Expected: PASS: import_std
// Test importing the std library with module qualification

import std;

test "import_std" {
    var a = std.abs_i64(-42);
    var b = std.max_i64(10, 20);
    var c = std.min_i64(5, 3);
    assert(a == 42);
    assert(b == 20);
    assert(c == 3);
}
