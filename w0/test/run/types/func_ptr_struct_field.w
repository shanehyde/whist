// Expected: PASS: func_ptr_struct_field

struct Callback { handler: func(i64) -> i64 }

func twice(x: i64) -> i64 {
    return x * 2;
}

test "func_ptr_struct_field" {
    var cb = new Callback { handler: twice };
    var result = cb.handler(5);
    assert(result == 10);
}
