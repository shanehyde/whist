// Expected error: Method 'map' expects 1 arguments, got 2

import collections;

func times_two(x: i64): i64 {
    return x * 2;
}

func main(): i32 {
    var nums = new Vec<i64>{};
    nums.map(times_two, times_two);
    return 0;
}
