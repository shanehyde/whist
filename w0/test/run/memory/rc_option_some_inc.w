// Expected: PASS: rc_option_some_inc
// Test that Option::Some(rc_value) correctly increments refcount.
// Regression test for issue #305: wrapping an RC-managed value from a Vec
// in Option::Some must emit __rc_inc to prevent double-free.

import std;

struct Opts {
    output_file: Option<string>,
}

test "rc_option_some_inc" {
    // Create a Vec that owns strings
    var args = new Vec<string>{"hello", "world"};

    // Wrap a string from the Vec in Option::Some — this must __rc_inc the string
    var no_string: Option<string> = Option::None;
    var opts = new Opts{ output_file: no_string };
    opts.output_file = Option::Some(args[0]);

    // The string should still be valid through both owners
    assert(opts.output_file.has_value());
    assert(opts.output_file.value() == "hello");

    // Both args Vec and opts go out of scope — without the fix, this double-frees

    std::println("PASS: rc_option_some_inc");
}
