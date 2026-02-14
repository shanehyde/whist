// Expected: PASS: vec_insert_remove_swap_remove
// Test Vec insert/remove/swap_remove behavior

test "vec_insert_remove_swap_remove" {
    var nums = new Vec<i64>{1, 2, 3};

    nums.insert(0, 10);
    nums.insert(2, 20);
    nums.insert(nums.count, 30);

    assert(nums.count == 6);
    assert(nums[0] == 10 && nums[1] == 1 && nums[2] == 20 && nums[3] == 2 && nums[4] == 3 &&
        nums[5] == 30);

    var removed_start = nums.remove(0);
    assert(removed_start == 10);

    var removed_mid = nums.remove(1);
    assert(removed_mid == 20);

    var removed_end = nums.remove(nums.count - 1);
    assert(removed_end == 30);

    assert(nums.count == 3 && nums[0] == 1 && nums[1] == 2 && nums[2] == 3);

    var swapped_start = nums.swap_remove(0);
    assert(swapped_start == 1);
    assert(nums.count == 2 && nums[0] == 3 && nums[1] == 2);

    var swapped_end = nums.swap_remove(1);
    assert(swapped_end == 2);
    assert(nums.count == 1 && nums[0] == 3);

    var swapped_last = nums.swap_remove(0);
    assert(swapped_last == 3 && nums.count == 0);
}
