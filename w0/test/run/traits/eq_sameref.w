// Expected: PASS: eq_sameref

struct Obj {
    val: i64,
}

test "eq_sameref" {
    var a = new Obj { val: 1 };
    var b = a;
    var c = new Obj { val: 1 };

    assert(sameref(a, b));
    assert(!sameref(a, c));
}
