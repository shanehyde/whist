// Expected: PASS: voidptr
// Test voidptr type

func identity(p: voidptr): voidptr {
    return p;
}

func get_null(): voidptr {
    return null;
}

test "voidptr" {
    // Declaration with null
    var p: voidptr = null;

    // Null comparison
    assert(p == null);

    // null == voidptr (reversed operands)
    assert(null == p);

    // voidptr equality
    var q: voidptr = null;
    assert(p == q);

    // Pass to and return from functions
    var r = identity(p);
    var s = get_null();

    // Assign null
    var t: voidptr = null;
    t = null;
}
