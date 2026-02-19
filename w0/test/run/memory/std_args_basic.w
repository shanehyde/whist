// RUNTIME TEST: std::args() includes argv[0] and is non-empty.

import std;

func main() -> i32 {
    var args = std::args();

    if (args.count < 1) {
        return 1;
    }

    if (args[0].length() == 0) {
        return 1;
    }

    return 0;
}
