// Expected: PASS: return_string_param
// Expected: PASS: return_struct_param
// Test returning RC parameters from functions (borrowed → owned)

import std;

func identity_string(s: string) -> string {
    return s;
}

struct Box { value: i64 }

func identity_box(b: Box) -> Box {
    return b;
}

test "return_string_param" {
    // Return string literal parameter
    var s = "hello";
    var t = identity_string(s);
    assert(t == "hello");

    // Return RC-managed string parameter (from concat)
    var s2 = "hel" + "lo";
    var t2 = identity_string(s2);
    assert(t2 == "hello");
}

test "return_struct_param" {
    // Return struct parameter
    var b = new Box{value: 42};
    var c: Box = identity_box(b);
    assert(c.value == 42);
}
