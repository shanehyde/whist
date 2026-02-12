// Test Vec first() and last() returning Option<T>

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};

    // first() on non-empty vec
    var f = nums.first();
    match (f) {
        Some(val) => {
            if (val != 10) { return 1; }
        },
        None => { return 2; },
    }

    // last() on non-empty vec
    var l = nums.last();
    match (l) {
        Some(val) => {
            if (val != 30) { return 3; }
        },
        None => { return 4; },
    }

    // Empty vec
    var empty = new Vec<i64>{};

    var ef = empty.first();
    match (ef) {
        Some(val) => { return 5; },
        None => {},
    }

    var el = empty.last();
    match (el) {
        Some(val) => { return 6; },
        None => {},
    }

    // Single element: first == last
    var one = new Vec<i64>{42};
    var of = one.first();
    var ol = one.last();
    match (of) {
        Some(val) => {
            if (val != 42) { return 7; }
        },
        None => { return 8; },
    }
    match (ol) {
        Some(val) => {
            if (val != 42) { return 9; }
        },
        None => { return 10; },
    }

    return 0;
}
