// Test basic Vec operations: create, push, index, count

func main(): i32 {
    var nums = new Vec<i64>{};

    nums.push(10);
    nums.push(20);
    nums.push(30);

    if (nums.count != 3) {
        return 1;
    }

    if (nums[0] != 10) {
        return 2;
    }

    if (nums[1] != 20) {
        return 3;
    }

    if (nums[2] != 30) {
        return 4;
    }

    return 0;
}
