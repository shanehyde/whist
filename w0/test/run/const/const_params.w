// Expected: PASS: const_params
// Test const parameters in function declarations

func add(const a: i64, const b: i64): i64 {
    return a + b;
}

func mixed(const readonly: i32, mutable: i32): i32 {
    mutable = mutable + 1;
    return readonly + mutable;
}

func identity(const x: i32): i32 {
    return x;
}

test "const_params" {
    var sum = add(10, 20);
    var result = mixed(5, 10);
    var val = identity(42);
}
