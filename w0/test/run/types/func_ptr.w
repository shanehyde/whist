// Expected: PASS: func_ptr_basic
// Expected: PASS: func_ptr_inferred
// Expected: PASS: func_ptr_method_ref
// Expected: PASS: func_ptr_nullable
// Expected: PASS: func_ptr_param
// Expected: PASS: func_ptr_struct_field

// --- Supporting definitions ---

func add(a: i64, b: i64) -> i64 {
    return a + b;
}

func greet() -> void {
    return;
}

struct Counter { value: i64 }

func (Counter) increment() -> void {
    self.value = self.value + 1;
}

func noop() -> void {
    return;
}

func apply(f: func(i64) -> i64, x: i64) -> i64 {
    return f(x);
}

func twice(x: i64) -> i64 {
    return x * 2;
}

struct Callback { handler: func(i64) -> i64 }

// --- Tests ---

test "func_ptr_basic" {
    var fp: func(i64, i64) -> i64 = add;
    var result = fp(2, 3);
    assert(result == 5);
}

test "func_ptr_inferred" {
    var fp = add;
    var result = fp(1, 2);
    assert(result == 3);
}

test "func_ptr_method_ref" {
    var f: func(Counter) -> void = Counter.increment;
}

test "func_ptr_nullable" {
    var fp: func() -> void = null;
    fp = noop;
}

test "func_ptr_param" {
    var result = apply(twice, 5);
    assert(result == 10);
}

test "func_ptr_struct_field" {
    var cb = new Callback { handler: twice };
    var result = cb.handler(5);
    assert(result == 10);
}
