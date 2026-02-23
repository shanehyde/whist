// Expected: PASS: return_new_struct_with_rc_param
// Expected: PASS: return_new_struct_with_string_param
// Expected: PASS: return_new_struct_with_struct_param
// Test that `return new Struct{field: param}` correctly increments RC fields (#313)

import std;

struct Token {
    value: string,
}

func make_token(s: string) -> Token {
    return new Token{ value: s };
}

struct Wrapper {
    inner: Token,
}

func wrap_token(t: Token) -> Wrapper {
    return new Wrapper{ inner: t };
}

struct Pair {
    a: string,
    b: string,
}

func make_pair(x: string, y: string) -> Pair {
    return new Pair{ a: x, b: y };
}

test "return_new_struct_with_string_param" {
    // String field from parameter — was use-after-free before fix
    var s = "hel" + "lo";
    var tok = make_token(s);
    assert(tok.value == "hello");
}

test "return_new_struct_with_struct_param" {
    // Struct field from parameter
    var tok = new Token{ value: "inner" };
    var w = wrap_token(tok);
    assert(w.inner.value == "inner");
}

test "return_new_struct_with_rc_param" {
    // Multiple RC fields from parameters
    var a = "foo" + "bar";
    var b = "baz" + "qux";
    var p = make_pair(a, b);
    assert(p.a == "foobar");
    assert(p.b == "bazqux");
}
