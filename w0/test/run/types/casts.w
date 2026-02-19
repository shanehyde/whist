// Expected: PASS: cast_char_int
// Expected: PASS: cast_struct_to_voidptr
// Expected: PASS: cast_voidptr_to_u64
// Expected: PASS: voidptr

// --- Supporting definitions ---

struct Box {
    value: i64,
}

func identity(p: voidptr) -> voidptr {
    return p;
}

func get_null() -> voidptr {
    return null;
}

// --- Tests ---

test "cast_char_int" {
    // char -> i32
    var ch: char = 'A';
    var code: i32 = ch as i32;
    assert(code == 65);

    // i32 -> char
    var num: i32 = 66;
    var letter: char = num as char;
    assert(letter == 'B');

    // Literal cast: char literal -> i32
    var val: i32 = 'Z' as i32;
    assert(val == 90);

    // Integer literal -> char
    var c2: char = 67 as char;
    assert(c2 == 'C');

    // Chained cast: char -> i32 -> char
    var original: char = 'D';
    var roundtrip: char = (original as i32) as char;
    assert(roundtrip == 'D');

    // Identity cast: i32 -> i32
    var x: i32 = 42;
    var y: i32 = x as i32;
    assert(y == 42);

    // Integer -> integer widening: i32 -> i64
    var small: i32 = 100;
    var big: i64 = small as i64;
    assert(big == 100);

    // Integer -> integer narrowing: i64 -> i32
    var wide: i64 = 200;
    var narrow: i32 = wide as i32;
    assert(narrow == 200);

    // Cast in expression context
    var digit: char = '5';
    var digit_val: i32 = digit as i32 - 48;
    assert(digit_val == 5);
}

test "cast_struct_to_voidptr" {
    var b = new Box { value: 42 };
    var p: voidptr = b as voidptr;
    assert(p != null);
}

test "cast_voidptr_to_u64" {
    var p: voidptr = null;
    var addr: u64 = p as u64;
    assert(addr == 0);
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
