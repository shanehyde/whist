import std;

func twice(x: i64): i64 {
    return x * 2;
}

func main(): i32 {
    // Basic variable interpolation
    var name: string = "world";
    var greeting = $"Hello {name}!";
    std.println(greeting);

    // Integer interpolation
    var age: i64 = 30;
    var msg = $"Age is {age}";
    std.println(msg);

    // Float interpolation
    var pi: f64 = 3.14;
    var fmsg = $"Pi is {pi}";
    std.println(fmsg);

    // Bool interpolation
    var flag: bool = true;
    var bmsg = $"Flag: {flag}";
    std.println(bmsg);
    // Expression interpolation
    var a: i64 = 10;
    var b: i64 = 20;
    var expr_msg = $"Sum: {a + b}";
    std.println(expr_msg);

    // Function call in interpolation
    var call_msg = $"Double 5 = {twice(5)}";
    std.println(call_msg);
    // Member access in interpolation
    var s: string = "test";
    var len_msg = $"Length is {s.length()}";
    std.println(len_msg);

    // Escaped braces
    var escaped = $"literal {{braces}}";
    std.println(escaped);

    // Adjacent interpolations
    var adj = $"{a}{b}";
    std.println(adj);

    // No expressions (plain string)
    var plain = $"hello";
    std.println(plain);

    // Empty string
    var empty = $"";
    std.println(empty);

    // Char interpolation
    var ch: char = 'A';
    var cmsg = $"Char: {ch}";
    std.println(cmsg);

    // Small integer types
    var small: i32 = 42;
    var smsg = $"Small: {small}";
    std.println(smsg);

    return 0;
}
