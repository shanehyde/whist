// Test parent directory imports (../)

import std;
import "./util/inner/helper.w";

func main(): i32 {
    var a = quadruple(3);

    if (a != 12) {
        print("FAIL: import_parent.w\n");
        return 1;
    }

    print("PASS: import_parent.w\n");
    return 0;
}
