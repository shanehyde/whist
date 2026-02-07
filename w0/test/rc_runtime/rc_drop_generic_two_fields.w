// RC RUNTIME TEST: Generic Pair<A,B> where both A and B have Drop

trait Drop {
    func drop(): void;
}

struct Alpha {
    id: i64,
}

impl Drop for Alpha {
    func (Alpha) drop(): void {
        // Alpha cleanup
    }
}

struct Beta {
    id: i64,
}

impl Drop for Beta {
    func (Beta) drop(): void {
        // Beta cleanup
    }
}

struct Pair<A, B> {
    first: A,
    second: B,
}

func main(): i32 {
    var a = new Alpha { id: 1 };
    var b = new Beta { id: 2 };
    var p = new Pair<Alpha, Beta> { first: a, second: b };
    return 0;
}
