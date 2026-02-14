// Generic struct with multiple type parameters

struct Pair<K, V> {
    first: K,
    second: V,
}

func main(): i32 {
    var p = new Pair<i64, i32> {first: 100, second: 42};
    if (p.first != 100) {
        return 1;
    }
    if (p.second != 42) {
        return 2;
    }
    return 0;
}
