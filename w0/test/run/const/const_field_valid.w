// Expected: PASS: const_field_valid
// Valid: const field can be set at construction and read

struct Box {
    const v: i64,
    name: string,
}

test "const_field_valid" {
    var b = new Box { v: 42, name: "hello" };
    var x = b.v;
    b.name = "world";
}
