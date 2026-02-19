// ERROR TEST: Struct in interpolation
// Expected error: String interpolation does not support type

struct Point { x: i64, y: i64 }

func main() -> i32 {
    var p = new Point {x: 1, y: 2};
    var s = $"Point: {p}";
    return 0;
}
