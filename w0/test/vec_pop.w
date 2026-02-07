// Test Vec pop operation

func main(): i32 {
    var nums = new Vec<i64>{10, 20, 30};

    var last = nums.pop();
    if (last != 30) {
        return 1;
    }

    if (nums.count != 2) {
        return 2;
    }

    var second = nums.pop();
    if (second != 20) {
        return 3;
    }

    var first = nums.pop();
    if (first != 10) {
        return 4;
    }

    if (nums.count != 0) {
        return 5;
    }

    return 0;
}
