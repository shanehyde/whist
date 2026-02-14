// RC RUNTIME TEST: Drop runs when an RC field is reassigned.
// Expected rc free order: 2 before 1

import std;

trait Drop {
    func drop(): void;
}

struct Child {
    value: i64,
}

struct Parent {
    child: Child,
}

impl Drop for Child {
    func drop(): void {
    }
}

func main(): i32 {
    var p = new Parent { child: new Child { value: 1 } };

    // Reassign with a fresh RC value; old child (alloc 1) should be freed
    // before the parent (alloc 2) at end of scope.
    p.child = new Child { value: 2 };

    return 0;
}
