import std;

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

func main(): i32 {
    var x: i32 = 42;
    var h: i32 = x.hash();
    std.print(std.format("hash of 42 = %d\n", h));

    var b: Bucket<i32> = new Bucket<i32>{key: 10};

    return 0;
}
