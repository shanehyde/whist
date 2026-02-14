// Expected: PASS: vec_init
// Test Vec initializer syntax: new Vec<T>{1, 2, 3}

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
