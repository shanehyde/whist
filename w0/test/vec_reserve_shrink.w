// Test Vec reserve and shrink_to_fit behavior

func main(): i32 {
    var nums = new Vec<i64>{1, 2, 3};

    if (nums.capacity != 4) {
        return 1;
    }

    nums.reserve(5);
    if (nums.count != 3) {
        return 2;
    }
    if (nums.capacity != 8) {
        return 3;
    }

    nums.reserve(0);
    if (nums.capacity != 8) {
        return 4;
    }

    nums.push(4);
    nums.push(5);
    nums.push(6);
    nums.push(7);
    nums.push(8);
    if (nums.count != 8) {
        return 5;
    }
    if (nums.capacity != 8) {
        return 6;
    }

    nums.pop();
    nums.pop();
    nums.pop();
    if (nums.count != 5) {
        return 7;
    }

    nums.shrink_to_fit();
    if (nums.capacity != 5) {
        return 8;
    }
    if (nums[4] != 5) {
        return 9;
    }

    nums.clear();
    nums.shrink_to_fit();
    if (nums.capacity != 0) {
        return 10;
    }

    return 0;
}
