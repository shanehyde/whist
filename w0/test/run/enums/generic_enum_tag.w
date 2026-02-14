// Test .tag access on generic enums
// Expected exit: 1

enum Option<T> {
    None,
    Some(T),
}

func main(): i32 {
    var s: Option<i64> = Option::Some(42);
    var n: Option<i64> = Option::None;
    var tag_s: i32 = s.tag;
    var tag_n: i32 = n.tag;

    if (tag_s != 1) { return 1; }
    if (tag_n != 0) { return 2; }

    return 0;
}
