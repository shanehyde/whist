// Test basic type aliases

type UserId = i64;
type Score = f64;
type Name = string;
type Flag = bool;

struct Point { x: i64, y: i64 }
type Pos = Point;

func add_ids(a: UserId, b: UserId): UserId {
    return a + b;
}

func main(): i32 {
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

    return 0;
}
