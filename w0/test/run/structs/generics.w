// Expected: PASS: generics_basic
// Expected: PASS: generics_methods
// Expected: PASS: generics_pair

struct Box<T> {
    value: T,
}

struct Pair<K, V> {
    key: K,
    value: V,
}

struct SimplePair<K, V> {
    first: K,
    second: V,
}

impl SimplePair<K, V> {
    func init(f: K, s: V) {
        self.first = f;
        self.second = s;
    }
}

func (Box<T>) get() -> T {
    return self.value;
}

func (Box<T>) set(v: T) -> void {
    self.value = v;
}

func (Pair<i32, Box<T>>) set(k: i32, v: Box<T>) -> void {
    self.key = k;
    self.value = v;
}

// --- generics_basic ---

test "generics_basic" {
    var b = new Box<i64> {value: 42};
    assert(b.value == 42);
}

// --- generics_methods ---

test "generics_methods" {
    var b = new Box<i64> {value: 10};
    var p = new Pair<i32, Box<i64>> {key: 1, value: b};

    // Test get method
    assert(b.get() == 10);

    // Test set method
    b.set(42);
    assert(b.get() == 42);
}

// --- generics_pair ---

test "generics_pair" {
    var p = new SimplePair<i64, i32>(100, 42);
    assert(p.first == 100);
    assert(p.second == 42);
}
