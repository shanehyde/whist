// Test that relative imports work but library symbols aren't re-exported
// double_abs uses std.abs_i64 internally, but we can't call abs_i64 directly

import "./util/std_helper.w";
import std;  // Need to import std ourselves to use std.print

func main(): i32 {
    // This should work - double_abs is defined in std_helper
    var a = double_abs(-5);

    if (a != 10) {
        std.print("FAIL: module_visibility.w - double_abs\n");
        return 1;
    }

    std.print("PASS: module_visibility.w\n");
    return 0;
}
