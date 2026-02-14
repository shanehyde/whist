// Expected: PASS: cast_struct_to_voidptr
// Test struct reference -> voidptr cast

struct Box {
    value: i64,
}

test "cast_struct_to_voidptr" {
    var b = new Box { value: 42 };
    var p: voidptr = b as voidptr;
    assert(p != null);
}
