// Test Vec initializer syntax: new Vec<T>{1, 2, 3}

func main(): i32 {
    var nums = new Vec<i64>{1, 2, 3};

    if (nums.count != 3) {
        return 1;
    }

    if (nums[0] != 1) {
        return 2;
    }

    if (nums[1] != 2) {
        return 3;
    }

    if (nums[2] != 3) {
        return 4;
    }

    // Push more after init
    nums.push(4);
    if (nums.count != 4) {
        return 5;
    }

    if (nums[3] != 4) {
        return 6;
    }

    return 0;
}
