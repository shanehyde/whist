// Expected error: return type mismatch
trait Drop {
    func drop() -> void;
}

struct Foo {
    x: i64,
}

impl Drop for Foo {
    func drop() -> i64 {
        return 0;
    }
}

func main() -> i32 {
    return 0;
}
