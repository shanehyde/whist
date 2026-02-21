// Expected: PASS: option_struct_field
// Test Option<string> as a struct field: method resolution and RC cleanup

import std;

struct Config {
    name: Option<string>,
    count: i64,
}

test "option_struct_field" {
    // Create struct with Some variant
    var cfg = new Config{ name: Option::Some("hello"), count: 42 };

    // Bug 1: method calls through struct field
    assert(cfg.name.has_value());
    assert(cfg.name.value() == "hello");
    assert(cfg.name.value_or("default") == "hello");

    // Pattern matching (already worked before this fix)
    match (cfg.name) {
        Some(v) => assert(v == "hello");
        None => assert(false);
    }

    // Test with None variant (direct inference from field type)
    var cfg2 = new Config{ name: Option::None, count: 0 };
    assert(!cfg2.name.has_value());
    assert(cfg2.name.value_or("default") == "default");

    match (cfg2.name) {
        Some(_) => assert(false);
        None => {}
    }

    // Bug 2: RC cleanup — struct with Option<string> must clean up correctly
    // (if cleanup emits wrong code, C compilation fails)

    std::println("PASS: option_struct_field");
}
