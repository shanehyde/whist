// Expected: PASS: use_basic
// Expected: PASS: use_grouped
// Expected: PASS: use_mixed
// Expected: PASS: use_type

import std;
use std::{abs_i64, max_i64};

test "use_basic" {
    var a = abs_i64(-42);
    assert(a == 42);
}

test "use_grouped" {
    var a = abs_i64(-42);
    var b = max_i64(10, 20);
    assert(a == 42);
    assert(b == 20);
}

test "use_mixed" {
    // Unqualified via use
    var a = abs_i64(-7);
    // Qualified still works
    var b = std::min_i64(3, 5);
    assert(a == 7);
    assert(b == 3);
}

test "use_type" {
    // Verify use works with function symbols
    var a = abs_i64(-99);
    assert(a == 99);
}
