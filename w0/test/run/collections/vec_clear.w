// Expected: PASS: vec_clear
// Test Vec clear and re-push

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
