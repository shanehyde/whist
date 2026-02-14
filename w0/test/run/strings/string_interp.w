// Expected: PASS: string_interp
import std;

func twice(x: i64): i64 {
    return x * 2;
}

test "string_interp" {
    // Basic variable interpolation
    var name: string = "world";
    var greeting = $"Hello {name}!";
    assert(greeting == "Hello world!");

    // Integer interpolation
    var age: i64 = 30;
    var msg = $"Age is {age}";
    assert(msg == "Age is 30");

    // Float interpolation
    var pi: f64 = 3.14;
    var fmsg = $"Pi is {pi}";
    // Float formatting may vary, just check it starts correctly
    assert(fmsg.starts_with("Pi is 3.14"));

    // Bool interpolation
    var flag: bool = true;
    var bmsg = $"Flag: {flag}";
    assert(bmsg == "Flag: true");

    // Expression interpolation
    var a: i64 = 10;
    var b: i64 = 20;
    var expr_msg = $"Sum: {a + b}";
    assert(expr_msg == "Sum: 30");

    // Function call in interpolation
    var call_msg = $"Double 5 = {twice(5)}";
    assert(call_msg == "Double 5 = 10");

    // Member access in interpolation
    var s: string = "test";
    var len_msg = $"Length is {s.length()}";
    assert(len_msg == "Length is 4");

    // Escaped braces
    var escaped = $"literal {{braces}}";
    assert(escaped == "literal {braces}");

    // Adjacent interpolations
    var adj = $"{a}{b}";
    assert(adj == "1020");

    // No expressions (plain string)
    var plain = $"hello";
    assert(plain == "hello");

    // Empty string
    var empty = $"";
    assert(empty == "");

    // Char interpolation
    var ch: char = 'A';
    var cmsg = $"Char: {ch}";
    assert(cmsg == "Char: A");

    // Small integer types
    var small: i32 = 42;
    var smsg = $"Small: {small}";
    assert(smsg == "Small: 42");
}
