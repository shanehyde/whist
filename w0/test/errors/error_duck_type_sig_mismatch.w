// Expected error: Method 'count' on 'Bag' return type mismatch

trait Countable {
    func count() -> i64;
}

struct Bag {
    n: i64,
}

// Signature-only declaration in impl
impl Countable for Bag {
    func count() -> i64;
}

// Standalone method has wrong return type
func (Bag) count() -> string {
    return "oops";
}

func main() -> i32 {
    return 0;
}
