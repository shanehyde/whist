// Test foreach over Vec<i64>

func main(): i32 {
    var nums = new Vec<i64>{};

    nums.push(10);
    nums.push(20);
    nums.push(30);

    var total: i64 = 0;
    foreach (const n in nums) {
        total = total + n;
    }

    if (total != 60) {
        return 1;
    }

    return 0;
}
