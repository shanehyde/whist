// Test Vec index write: v[i] = x

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};

    nums[1] = 99;

    if (nums[0] != 10) {
        return 1;
    }

    if (nums[1] != 99) {
        return 2;
    }

    if (nums[2] != 30) {
        return 3;
    }

    return 0;
}
