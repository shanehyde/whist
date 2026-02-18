// Expected: PASS: module_visibility
// Test that relative imports work but library symbols aren't re-exported
// double_abs uses std::abs_i64 internally, but we can't call abs_i64 directly

import "./util/std_helper.w";

test "module_visibility" {
    // This should work - double_abs is defined in std_helper
    var a = double_abs(-5);
    assert(a == 10);
}
