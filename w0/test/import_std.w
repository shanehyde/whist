// Test importing the std library

import std;

extern stdio {
    func printf(s: string): void;
}

func main(): i32 {
    var a = abs_i64(-42);
    var b = max_i64(10, 20);
    var c = min_i64(5, 3);

    if (a != 42 || b != 20 || c != 3) {
        printf("FAIL: import_std.w\n");
        return 1;
    }

    printf("PASS: import_std.w\n");
    return 0;
}
