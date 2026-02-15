// Expected: PASS: rc_strings
// Expected: PASS: rc_strings2
// Test RC string memory management

import std;

func make_greeting(name: string): string {
    return "Hello, " + name + "!";
}

struct Person {
    name: string,
    title: string,
}

test "rc_strings" {
    // String literals are immortal (no alloc/free)
    var lit = "hello";
    assert(lit == "hello");

    // String concat creates RC-managed string
    var s = "foo" + "bar";
    assert(s == "foobar");

    // String substr creates RC-managed string
    var sub = s[0:3];
    assert(sub == "foo");

    // String interpolation with expressions creates RC-managed string
    var x = 42;
    var interp = $"x is {x}";
    assert(interp == "x is 42");

    // Function returning string transfers ownership
    var greeting = make_greeting("World");
    assert(greeting == "Hello, World!");

    // String copy between variables
    var a = "dynamic" + "!";
    var b = a;
    assert(a == b);

    // Struct with string fields (cleanup decrements strings)
    var p = new Person { name: "Alice", title: "Dr." };
    assert(p.name == "Alice");
    assert(p.title == "Dr.");
    // p = null;

    // Vec<string> with dynamic strings
    var parts = "one,two,three".split(",");
    assert(parts.count == 3);
    assert(parts[0] == "one");
    assert(parts[1] == "two");
    assert(parts[2] == "three");
    // parts.clear(); // Clear should free strings

    // std.format creates RC-managed string
    var formatted = std.format("value=%d", 99);
    assert(formatted == "value=99");
}

// impl Drop for Vec<string> {
//     func drop() {
//         // Clear should free strings
//         self.clear();
//     }
// }

test "rc_strings2" {
    // Vec<string> with dynamic strings
    var parts = "one,two,three".split(",");
    assert(parts.count == 3);
    assert(parts[0] == "one");
    assert(parts[1] == "two");
    assert(parts[2] == "three");
    // parts.clear(); // Clear should free strings
}
