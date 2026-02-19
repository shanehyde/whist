// Test that symbols from library imports are not accessible through relative imports
// Expected error: Undefined identifier 'abs_i64'

import "../run/modules/util/std_helper.w";

func main() -> i32 {
    // This should work - double_abs is defined in std_helper
    var a = double_abs(-5);

    // This should FAIL - abs_i64 is from std, which was imported by std_helper
    // but not by this file
    var b = abs_i64(-10);

    return 0;
}
