// Test importing the std library

import std;

func main(): i32 {
    var a = abs_i64(-42);
    var b = max_i64(10, 20);
    var c = min_i64(5, 3);

    if (a != 42 || b != 20 || c != 3) {
        print("FAIL: import_std.w\n");
        return 1;
    }

    print("PASS: import_std.w\n");
    return 0;
}
