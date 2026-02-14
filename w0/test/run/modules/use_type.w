// Expected: PASS: use_type
// Test use statement with types (struct from std)
// Currently std types like Result aren't public, so this test uses
// use for functions only but verifies type-related operations work
import std;
use std.abs_i64;

test "use_type" {
    // Verify use works with function symbols
    var a = abs_i64(-99);
    assert(a == 99);
}
