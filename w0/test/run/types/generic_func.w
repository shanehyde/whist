// Expected: PASS: generic_func_basic
// Expected: PASS: generic_func_bounds
// Expected: PASS: generic_func_multi_param
// Expected: PASS: generic_func_vec

// --- Supporting definitions ---

func identity<T>(x: T) -> T {
    return x;
}

trait Printable {
    const func label() -> string;
}

struct Dog {
    name: string,
}

impl Printable for Dog {
    const func label() -> string {
        return self.name;
    }
}

func get_label<T: Printable>(x: T) -> string {
    return x.label();
}

func first<A, B>(a: A, b: B) -> A {
    return a;
}

func second<A, B>(a: A, b: B) -> B {
    return b;
}

func vec_count<T>(v: Vec<T>) -> i64 {
    return v.count;
}

// --- Tests ---

test "generic_func_basic" {
    var a = identity(42);
    var b = identity("hello");
    var c = identity(true);

    assert(a == 42);
    assert(b == "hello");
    assert(c == true);
}

test "generic_func_bounds" {
    var d = new Dog{name: "Rex"};
    var s = get_label(d);
    assert(s == "Rex");
}

test "generic_func_multi_param" {
    var a = first(10, "hello");
    var b = second(10, "world");

    assert(a == 10);
    assert(b == "world");
}

test "generic_func_vec" {
    var v = new Vec<i64>{1, 2, 3};

    var n = vec_count(v);
    assert(n == 3);
}
