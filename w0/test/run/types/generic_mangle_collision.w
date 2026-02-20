// Expected: PASS: generic_mangle_collision
// Test that array, tuple, and function types produce distinct mangled names
// for generic instantiation, preventing collision in the checker's cache.

struct Wrapper<T> {
    value: T,
}

func add_one(x: i64) -> i64 {
    return x + 1;
}

func is_positive(x: i64) -> bool {
    return x > 0;
}

func negate(x: i64) -> i64 {
    return -x;
}

test "generic_mangle_collision" {
    // Different function types as generic arguments.
    // Before the fix, all mangled to Wrapper_unknown causing collisions.
    // Now: Wrapper_fn_i64_r_i64, Wrapper_fn_i64_r_bool
    var a = new Wrapper<func(i64) -> i64> { value: add_one };
    var b = new Wrapper<func(i64) -> bool> { value: is_positive };

    assert(a.value(41) == 42);
    assert(b.value(1) == true);
    assert(b.value(-1) == false);

    // A second instantiation with same signature should reuse correctly
    var c = new Wrapper<func(i64) -> i64> { value: negate };
    assert(c.value(5) == -5);

    // Basic type coexists with function types
    var d = new Wrapper<i64> { value: 100 };
    assert(d.value == 100);
}
