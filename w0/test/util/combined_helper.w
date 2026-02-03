// Helper that imports another relative module to test namespace merging

import "./math_helper.w";

// This should work because math_helper symbols are merged into our namespace
func apply_both(x: i64): i64 {
    var result = add_two(x);
    return multiply_three(result);
}
