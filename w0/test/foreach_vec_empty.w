// Test foreach over empty Vec iterates zero times

func main(): i32 {
    var nums = new Vec<i64>{};

    var count: i64 = 0;
    foreach (const n in nums) {
        count = count + 1;
    }

    if (count != 0) {
        return 1;
    }

    return 0;
}
