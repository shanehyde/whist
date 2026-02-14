// Expected: PASS: vec_index_write
// Test Vec index write: v[i] = x

test "vec_index_write" {
    var nums = new Vec<i64>{10, 20, 30};

    nums[1] = 99;

    assert(nums[0] == 10);
    assert(nums[1] == 99);
    assert(nums[2] == 30);
}
