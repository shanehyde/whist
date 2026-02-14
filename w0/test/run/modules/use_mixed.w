// Expected: PASS: use_mixed
// Test use with both types and functions, and qualified access still working
import std;
use std.abs_i64;

test "use_mixed" {
    // Unqualified via use
    var a = abs_i64(-7);
    // Qualified still works
    var b = std.min_i64(3, 5);
    assert(a == 7);
    assert(b == 3);
}
