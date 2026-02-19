// RC RUNTIME TEST: Drop runs before field cleanup
// Expected rc free order: 1 before 2

import std;

trait Drop {
    func drop() -> void;
}

struct Inner {
    value: i64,
}

struct Container {
    child: Inner,
}

impl Drop for Container {
    func drop() -> void {
        std::print("DROP_CALLED\n");
    }
}

func main() -> i32 {
    var c = new Inner { value: 10 };
    var p = new Container { child: c };
    return 0;
}
