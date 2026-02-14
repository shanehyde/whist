// Test generic type aliases

struct Box<T> { value: T }
struct Pair<K, V> { key: K, value: V }

// Partial application: fix one type parameter
type IntBox = Box<i64>;
type StringPair<V> = Pair<string, V>;

func main(): i32 {
    // Use non-generic alias of a generic struct
    var b: IntBox = new Box<i64> { value: 42 };

    // Use generic alias with partial application
    var p: StringPair<i64> = new Pair<string, i64> { key: "age", value: 30 };

    // The aliased type is fully interchangeable
    var b2: Box<i64> = b;
    var p2: Pair<string, i64> = p;

    return 0;
}
