// Expected: PASS: vec_reserve_shrink
// Test Vec reserve and shrink_to_fit behavior

test "vec_reserve_shrink" {
    var nums = new Vec<i64>{1, 2, 3};

    assert(nums.capacity == 4);

    nums.reserve(5);
    assert(nums.count == 3);
    assert(nums.capacity == 8);

    nums.reserve(0);
    assert(nums.capacity == 8);

    nums.push(4);
    nums.push(5);
    nums.push(6);
    nums.push(7);
    nums.push(8);
    assert(nums.count == 8);
    assert(nums.capacity == 8);

    nums.pop();
    nums.pop();
    nums.pop();
    assert(nums.count == 5);

    nums.shrink_to_fit();
    assert(nums.capacity == 5);
    assert(nums[4] == 5);

    nums.clear();
    nums.shrink_to_fit();
    assert(nums.capacity == 0);
}
