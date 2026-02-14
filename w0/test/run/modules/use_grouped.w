// Expected: PASS: use_grouped
// Test grouped use statement for multiple functions
import std;
use std.{abs_i64, max_i64};

test "use_grouped" {
    var a = abs_i64(-42);
    var b = max_i64(10, 20);
    assert(a == 42);
    assert(b == 20);
}
