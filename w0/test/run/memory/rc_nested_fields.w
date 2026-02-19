// RC RUNTIME TEST: Nested RC fields are freed when parent is freed
// Expected rc free order: 1 before 2

struct Inner {
    value: i64,
}

struct Outer {
    child: Inner,
}

func main() -> i32 {
    var inner = new Inner { value: 42 };
    var outer = new Outer { child: inner };
    return 0;
}
