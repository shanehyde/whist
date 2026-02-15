// Expected: PASS: type_alias
// Expected: PASS: type_alias_generic

// --- Supporting definitions ---

type UserId = i64;
type Score = f64;
type Name = string;
type Flag = bool;

struct Point { x: i64, y: i64 }
type Pos = Point;

func add_ids(a: UserId, b: UserId): UserId {
    return a + b;
}

struct Box<T> { value: T }
struct Pair<K, V> { key: K, value: V }

type IntBox = Box<i64>;
type StringPair<V> = Pair<string, V>;

// --- Tests ---

test "type_alias" {
    var id: UserId = 42;
    var id2: UserId = 100;
    var total: UserId = add_ids(id, id2);

    var score: Score = 3.14;
    var name: Name = "hello";
    var flag: Flag = true;

    // Alias is interchangeable with the underlying type
    var raw: i64 = id;
    var id3: UserId = raw;

    // Struct alias
    var p: Pos = new Point { x: 1, y: 2 };
    var q: Point = p;
}

test "type_alias_generic" {
    // Use non-generic alias of a generic struct
    var b: IntBox = new Box<i64> { value: 42 };

    // Use generic alias with partial application
    var p: StringPair<i64> = new Pair<string, i64> { key: "age", value: 30 };

    // The aliased type is fully interchangeable
    var b2: Box<i64> = b;
    var p2: Pair<string, i64> = p;
}
