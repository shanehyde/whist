trait Hashable {
    const func hash() -> u32;
}

// i64 does NOT implement Hashable

struct Bucket<K: Hashable> {
    key: K,
}

func main() -> i32 {
    // Expected error: does not implement trait
    var b: Bucket<i64> = new Bucket<i64>{key: 10};
    return 0;
}
