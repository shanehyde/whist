// RC RUNTIME TEST: Drop runs when an RC field is reassigned.

trait Drop {
    func drop(): void;
}

var drop_count: i64 = 0;

struct Child {
    value: i64,
}

struct Parent {
    child: Child,
}

impl Drop for Child {
    func (Child) drop(): void {
        drop_count = drop_count + 1;
    }
}

func main(): i32 {
    var p = new Parent { child: new Child { value: 1 } };

    // Reassign with a fresh RC value; old child should drop now.
    p.child = new Child { value: 2 };

    if (drop_count != 1) {
        return 1;
    }

    return 0;
}
