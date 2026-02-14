// Expected: PASS: vec_pop
// Test Vec pop operation -- returns Option<T>

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
