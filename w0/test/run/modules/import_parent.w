// Expected: PASS: import_parent
// Test parent directory imports (../)

import "./util/inner/helper.w";

test "import_parent" {
    var a = quadruple(3);
    assert(a == 12);
}
