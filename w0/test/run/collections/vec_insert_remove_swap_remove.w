// Test Vec insert/remove/swap_remove behavior

func main(): i32 {
    var nums = new Vec<i64>{1, 2, 3};

    nums.insert(0, 10);
    nums.insert(2, 20);
    nums.insert(nums.count, 30);

    if (nums.count != 6) {
        return 1;
    }
    if (nums[0] != 10 || nums[1] != 1 || nums[2] != 20 || nums[3] != 2 || nums[4] != 3 ||
        nums[5] != 30) {
        return 2;
    }

    var removed_start = nums.remove(0);
    if (removed_start != 10) {
        return 3;
    }

    var removed_mid = nums.remove(1);
    if (removed_mid != 20) {
        return 4;
    }

    var removed_end = nums.remove(nums.count - 1);
    if (removed_end != 30) {
        return 5;
    }

    if (nums.count != 3 || nums[0] != 1 || nums[1] != 2 || nums[2] != 3) {
        return 6;
    }

    var swapped_start = nums.swap_remove(0);
    if (swapped_start != 1) {
        return 7;
    }
    if (nums.count != 2 || nums[0] != 3 || nums[1] != 2) {
        return 8;
    }

    var swapped_end = nums.swap_remove(1);
    if (swapped_end != 2) {
        return 9;
    }
    if (nums.count != 1 || nums[0] != 3) {
        return 10;
    }

    var swapped_last = nums.swap_remove(0);
    if (swapped_last != 3 || nums.count != 0) {
        return 11;
    }

    return 0;
}
