// Expected: PASS: traits_primitive

trait Hashable {
    const func hash(): i32;
}

impl Hashable for i32 {
    const func hash(): i32 {
        return self;
    }
}

struct Bucket<K: Hashable> {
    key: K,
}

test "traits_primitive" {
    var x: i32 = 42;
    var h: i32 = x.hash();
    assert(h == 42);

    var b: Bucket<i32> = new Bucket<i32>{key: 10};
}
