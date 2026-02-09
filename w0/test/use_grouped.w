// Test grouped use statement for multiple functions
import std;
use std.{print, abs_i64, max_i64};

func main(): i32 {
    var a = abs_i64(-42);
    var b = max_i64(10, 20);

    if (a != 42 || b != 20) {
        print("FAIL: use_grouped.w\n");
        return 1;
    }

    print("PASS: use_grouped.w\n");
    return 0;
}
