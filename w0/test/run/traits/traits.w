// Expected: PASS: traits_basic
// Expected: PASS: traits_bounds
// Expected: PASS: traits_primitive

trait Greetable {
    func greet() -> string;
    const func name_length() -> i64;
}

struct Dog {
    name: string,
}

impl Greetable for Dog {
    func greet() -> string {
        return self.name;
    }
    const func name_length() -> i64 {
        return 3;
    }
}

trait HasValue {
    func value() -> i64;
}

struct Wrapper {
    v: i64,
}

impl HasValue for Wrapper {
    func value() -> i64 {
        return self.v;
    }
}

struct Box<T: HasValue> {
    item: T,
}

trait Hashable {
    const func hash() -> i32;
}

impl Hashable for i32 {
    const func hash() -> i32 {
        return self;
    }
}

struct Bucket<K: Hashable> {
    key: K,
}

test "traits_basic" {
    var d: Dog = new Dog{name: "Rex\n"};
    var s: string = d.greet();
}

test "traits_bounds" {
    var w = new Wrapper {v: 42};
    var b = new Box<Wrapper> {item: w};
    assert(w.value() == 42);
}

test "traits_primitive" {
    var x: i32 = 42;
    var h: i32 = x.hash();
    assert(h == 42);

    var b: Bucket<i32> = new Bucket<i32>{key: 10};
}
