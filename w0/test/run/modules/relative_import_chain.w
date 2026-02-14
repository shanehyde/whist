// Expected: PASS: relative_import_chain
// Test that relative imports correctly merge namespaces across a chain of imports
// combined_helper imports math_helper, and we import combined_helper
// We should be able to call functions from both files directly

import "./util/combined_helper.w";

test "relative_import_chain" {
    // This calls a function from combined_helper.w
    var a = apply_both(5);  // (5 + 2) * 3 = 21

    // These call functions from math_helper.w (merged through combined_helper)
    var b = add_two(10);        // 12
    var c = multiply_three(4);  // 12

    assert(a == 21);
    assert(b == 12);
    assert(c == 12);
}
