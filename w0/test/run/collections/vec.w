// Expected: PASS: vec_basic
// Expected: PASS: vec_clear
// Expected: PASS: vec_init
// Expected: PASS: vec_index_write
// Expected: PASS: vec_pop
// Expected: PASS: vec_first_last
// Expected: PASS: vec_span
// Expected: PASS: vec_nested
// Expected: PASS: vec_contains_sort
// Expected: PASS: vec_string_sort
// Expected: PASS: vec_user_method

import collections;

func (Vec<T>) beep(value: T) -> bool {
    for (var i: i64 = 0; i < self.count; i += 1) {
        if (self[i] == value) {
            return true;
        }
    }
    return false;
}

// Shared definitions

func sum(s: Span<i64>) -> i64 {
    var total: i64 = 0;
    for (var i: i64 = 0; i < s.count; i += 1) {
        total = total + s[i];
    }
    return total;
}

// Tests

test "vec_basic" {
    var nums = new Vec<i64>{};

    nums.push(10);
    nums.push(20);
    nums.push(30);

    assert(nums.count == 3);
    assert(nums[0] == 10);
    assert(nums[1] == 20);
    assert(nums[2] == 30);
}

test "vec_clear" {
    var nums = new Vec<i64>{1, 2, 3};

    assert(nums.count == 3);

    nums.clear();

    assert(nums.count == 0);

    // Re-push after clear
    nums.push(42);
    nums.push(99);

    assert(nums.count == 2);
    assert(nums[0] == 42);
    assert(nums[1] == 99);
}

test "vec_init" {
    var nums = new Vec<i64>{1, 2, 3};

    assert(nums.count == 3);
    assert(nums[0] == 1);
    assert(nums[1] == 2);
    assert(nums[2] == 3);

    // Push more after init
    nums.push(4);
    assert(nums.count == 4);
    assert(nums[3] == 4);
}

test "vec_index_write" {
    var nums = new Vec<i64>{10, 20, 30};

    nums[1] = 99;

    assert(nums[0] == 10);
    assert(nums[1] == 99);
    assert(nums[2] == 30);
}

test "vec_pop" {
    var nums = new Vec<i64>{10, 20, 30};

    // Pop returns Some(last_element)
    var last = nums.pop();
    match (last) {
        Some(v) => {
            assert(v == 30);
        },
        None => { assert(false); },
    }

    assert(nums.count == 2);

    // Pop second element
    var second = nums.pop();
    match (second) {
        Some(v) => {
            assert(v == 20);
        },
        None => { assert(false); },
    }

    // Pop first element
    var first = nums.pop();
    match (first) {
        Some(v) => {
            assert(v == 10);
        },
        None => { assert(false); },
    }

    assert(nums.count == 0);

    // Pop from empty Vec returns None
    var empty_pop = nums.pop();
    match (empty_pop) {
        Some(v) => { assert(false); },
        None => {},
    }
}

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

test "vec_span" {
    var nums = new Vec<i64>{10, 20, 30, 40, 50};

    // Full slice
    var all: Span<i64> = nums[:];
    assert(sum(all) == 150);

    // Partial slice
    var mid: Span<i64> = nums[1:4];
    assert(sum(mid) == 90);

    // Start-only slice
    var tail: Span<i64> = nums[3:];
    assert(sum(tail) == 90);
}

test "vec_nested" {
    var outer = new Vec<Vec<i64>>{};

    var inner1 = new Vec<i64>{1, 2, 3};
    var inner2 = new Vec<i64>{4, 5};

    outer.push(inner1);
    outer.push(inner2);

    assert(outer.count == 2);
    assert(outer[0].count == 3);
    assert(outer[1].count == 2);
    assert(outer[0][0] == 1);
    assert(outer[0][2] == 3);
    assert(outer[1][1] == 5);

    // Push to inner vec after it's in outer
    outer[0].push(10);

    assert(outer[0].count == 4);
    assert(outer[0][3] == 10);

    // outer goes out of scope -- should __rc_dec each inner Vec
}

test "vec_contains_sort" {
    var values = new Vec<i64>{3, 1, 2};

    assert(values.contains(2));
    assert(!values.contains(99));

    var empty = new Vec<i64>{};
    assert(!empty.contains(1));

    values.sort();
    assert(values[0] == 1);
    assert(values[1] == 2);
    assert(values[2] == 3);

    var already_sorted = new Vec<i64>{1, 2, 3};
    already_sorted.sort();
    assert(already_sorted[0] == 1 && already_sorted[1] == 2 && already_sorted[2] == 3);

    var reverse = new Vec<i64>{5, 4, 3, 2, 1};
    reverse.sort();
    assert(reverse[0] == 1 && reverse[1] == 2 && reverse[2] == 3 && reverse[3] == 4 &&
        reverse[4] == 5);
}

test "vec_string_sort" {
    var v = new Vec<string>{"cherry", "apple", "banana"};
    v.sort();
    assert(v[0] == "apple");
    assert(v[1] == "banana");
    assert(v[2] == "cherry");

    // Empty vec
    var empty = new Vec<string>{};
    empty.sort();
    assert(empty.count == 0);

    // Single element
    var single = new Vec<string>{"only"};
    single.sort();
    assert(single[0] == "only");

    // Already sorted
    var sorted = new Vec<string>{"a", "b", "c"};
    sorted.sort();
    assert(sorted[0] == "a" && sorted[1] == "b" && sorted[2] == "c");

    // Reverse order
    var rev = new Vec<string>{"z", "m", "a"};
    rev.sort();
    assert(rev[0] == "a" && rev[1] == "m" && rev[2] == "z");
}

test "vec_user_method" {
    var nums = new Vec<i64>{10, 20, 30};

    assert(nums.beep(20));
    assert(!nums.beep(99));

    // Empty vec
    var empty = new Vec<i64>{};
    assert(!empty.beep(1));

    // String vec with same user method
    var words = new Vec<string>{"hello", "world"};
    assert(words.beep("hello"));
    assert(!words.beep("missing"));
}

func a10(f:i64) -> bool {
    return f % 2 == 0;
}

test "vec_user_method_nested" {
    var v = new Vec<i64>{2,4,6,8,10};
    assert(v.all(a10));
}
