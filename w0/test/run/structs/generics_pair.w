// Expected: PASS: generics_pair
// Generic struct with multiple type parameters

struct Pair<K, V> {
    first: K,
    second: V,
}

test "generics_pair" {
    var p = new Pair<i64, i32> {first: 100, second: 42};
    assert(p.first == 100);
    assert(p.second == 42);
}
