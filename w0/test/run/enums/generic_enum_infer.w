// Test type inference for generic enums
// Expected exit: 1

enum Option<T> {
    None,
    Some(T),
}

func main() -> i32 {
    // Infer T from constructor argument
    var x = Option::Some(42);
    var y = Option::Some(3.14);
    var z = Option::Some("hello");

    if (x.tag != 1) { return 1; }
    if (y.tag != 1) { return 2; }
    if (z.tag != 1) { return 3; }

    return 0;
}
