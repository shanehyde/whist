// Expected: PASS: option_default_none
// Test that Option<T> defaults to None when not initialized

import std;

struct Config {
    name: Option<string>,
    count: i64,
}

struct Point {
    x: i64,
    y: i64,
    label: Option<string>,
    tag: Option<i64>,
}

test "option_default_none" {
    // 1. Struct field: omit Option<string> field
    var cfg = new Config{ count: 42 };
    assert(!cfg.name.has_value());
    assert(cfg.name.value_or("default") == "default");

    // 2. Struct field: omit multiple Option fields
    var pt = new Point{ x: 1, y: 2 };
    assert(!pt.label.has_value());
    assert(!pt.tag.has_value());
    assert(pt.tag.value_or(-1) == -1);

    // 3. Struct field: mix explicit and defaulted
    var pt2 = new Point{ x: 10, y: 20, label: Option::Some("origin") };
    assert(pt2.label.has_value());
    assert(pt2.label.value() == "origin");
    assert(!pt2.tag.has_value());

    // 4. Variable: Option<i64> without initializer
    var opt_int: Option<i64>;
    assert(!opt_int.has_value());
    assert(opt_int.value_or(99) == 99);

    // 5. Variable: Option<string> without initializer
    var opt_str: Option<string>;
    assert(!opt_str.has_value());
    assert(opt_str.value_or("none") == "none");

    // 6. Pattern matching on defaulted values
    match (cfg.name) {
        Some(_) => assert(false);
        None => {}
    }

    match (opt_int) {
        Some(_) => assert(false);
        None => {}
    }

    std::println("PASS: option_default_none");
}
