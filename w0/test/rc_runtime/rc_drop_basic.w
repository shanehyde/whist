// RC RUNTIME TEST: Drop method is called when refcount reaches 0

import std;

trait Drop {
    func drop(): void;
}

struct Resource {
    id: i64,
}

impl Drop for Resource {
    func (Resource) drop(): void {
        // Drop method called - resource cleaned up
    }
}

func main(): i32 {
    var r = new Resource { id: 1 };
    return 0;
}
