// Test voidptr type

func identity(p: voidptr): voidptr {
    return p;
}

func get_null(): voidptr {
    return null;
}

func main(): i32 {
    // Declaration with null
    var p: voidptr = null;

    // Null comparison
    if (p == null) {
        var x = 1;
    }
    if (p != null) {
        var x = 2;
    }

    // null == voidptr (reversed operands)
    if (null == p) {
        var x = 3;
    }

    // voidptr equality
    var q: voidptr = null;
    if (p == q) {
        var x = 4;
    }

    // Pass to and return from functions
    var r = identity(p);
    var s = get_null();

    // Assign null
    var t: voidptr = null;
    t = null;

    return 0;
}
