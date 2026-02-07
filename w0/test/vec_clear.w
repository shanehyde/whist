// Test Vec clear and re-push

func main(): i32 {
    var nums = new Vec<i64>{1, 2, 3};

    if (nums.count != 3) {
        return 1;
    }

    nums.clear();

    if (nums.count != 0) {
        return 2;
    }

    // Re-push after clear
    nums.push(42);
    nums.push(99);

    if (nums.count != 2) {
        return 3;
    }

    if (nums[0] != 42) {
        return 4;
    }

    if (nums[1] != 99) {
        return 5;
    }

    return 0;
}
