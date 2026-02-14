// Valid: const field can be set at construction and read

struct Box {
    const v: i64,
    name: string,
}

func main(): i32 {
    var b = new Box { v: 42, name: "hello" };
    var x = b.v;
    b.name = "world";
    return 0;
}
