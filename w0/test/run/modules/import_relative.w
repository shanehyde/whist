// Expected: PASS: import_relative
// Test relative imports

import "./util/math.w";

test "import_relative" {
    var a = twice(5);
    var b = triple(4);
    assert(a == 10);
    assert(b == 12);
}
