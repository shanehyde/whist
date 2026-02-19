// Tests for Vec is_empty, find, extend methods

import std;

// Expected: all passed

func main() -> i32 {
    // --- is_empty ---
    var empty = new Vec<i64>{};
    assert(empty.is_empty());

    var non_empty = new Vec<i64>{1, 2, 3};
    assert(!non_empty.is_empty());

    // --- find ---
    var nums = new Vec<i64>{10, 20, 30, 40};

    var found = nums.find(|x| x > 25);
    assert(found.has_value());
    assert(found.value() == 30);

    var not_found = nums.find(|x| x > 100);
    assert(!not_found.has_value());

    // --- extend ---
    var a = new Vec<i64>{1, 2};
    var b = new Vec<i64>{3, 4, 5};
    a.extend(b);
    assert(a.count == 5);
    assert(a[0] == 1);
    assert(a[2] == 3);
    assert(a[4] == 5);

    // extend with empty
    var c = new Vec<i64>{10};
    var d = new Vec<i64>{};
    c.extend(d);
    assert(c.count == 1);

    std::println("all passed");
    return 0;
}
