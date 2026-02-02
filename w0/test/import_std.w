// Test importing the std library with module qualification

import std;

func main(): i32 {
    var a = std.abs_i64(-42);
    var b = std.max_i64(10, 20);
    var c = std.min_i64(5, 3);

    if (a != 42 || b != 20 || c != 3) {
        std.print("FAIL: import_std.w\n");
        return 1;
    }

    std.print("PASS: import_std.w\n");
    return 0;
}
