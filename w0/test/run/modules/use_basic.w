// Expected: PASS: use_basic
// Test basic use statement for a single function
import std;
use std.abs_i64;

test "use_basic" {
    var a = abs_i64(-42);
    assert(a == 42);
}
