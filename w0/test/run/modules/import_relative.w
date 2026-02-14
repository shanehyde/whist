// Test relative imports

import std;
import "./util/math.w";

func main(): i32 {
    var a = twice(5);
    var b = triple(4);

    if (a != 10 || b != 12) {
        std.print("FAIL: import_relative.w\n");
        return 1;
    }

    std.print("PASS: import_relative.w\n");
    return 0;
}
