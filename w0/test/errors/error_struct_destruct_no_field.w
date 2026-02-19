// ERROR TEST: Struct destructuring with non-existent field
// Expected error: Struct 'Point' has no field 'z'

struct Point {
    x: i64,
    y: i64,
}

func main() -> i32 {
    var {x, z} = new Point {x: 1, y: 2};
    return 0;
}
