// Test use with both types and functions, and qualified access still working
import std;
use std.{print, abs_i64};

func main(): i32 {
    // Unqualified via use
    var a = abs_i64(-7);
    // Qualified still works
    var b = std.min_i64(3, 5);

    if (a != 7 || b != 3) {
        print("FAIL: use_mixed.w\n");
        return 1;
    }

    print("PASS: use_mixed.w\n");
    return 0;
}
