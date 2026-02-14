// Expected: PASS: vec_contains_sort
// Test Vec contains() and sort()

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
