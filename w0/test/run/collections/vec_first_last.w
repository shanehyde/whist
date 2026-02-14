// Expected: PASS: vec_first_last
// Test Vec first() and last() returning Option<T>

test "vec_first_last" {
    var nums = new Vec<i64>{10, 20, 30};

    // first() on non-empty vec
    var f = nums.first();
    match (f) {
        Some(val) => {
            assert(val == 10);
        },
        None => { assert(false); },
    }

    // last() on non-empty vec
    var l = nums.last();
    match (l) {
        Some(val) => {
            assert(val == 30);
        },
        None => { assert(false); },
    }

    // Empty vec
    var empty = new Vec<i64>{};

    var ef = empty.first();
    match (ef) {
        Some(val) => { assert(false); },
        None => {},
    }

    var el = empty.last();
    match (el) {
        Some(val) => { assert(false); },
        None => {},
    }

    // Single element: first == last
    var one = new Vec<i64>{42};
    var of = one.first();
    var ol = one.last();
    match (of) {
        Some(val) => {
            assert(val == 42);
        },
        None => { assert(false); },
    }
    match (ol) {
        Some(val) => {
            assert(val == 42);
        },
        None => { assert(false); },
    }
}
