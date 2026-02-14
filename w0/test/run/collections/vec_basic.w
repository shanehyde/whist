// Expected: PASS: vec_basic
// Test basic Vec operations: create, push, index, count

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
