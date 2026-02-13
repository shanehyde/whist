// Expected error: Type 'Point' has no method 'nonexistent'
struct Point { x: i64, y: i64 }

func main(): i32 {
    var f = Point.nonexistent;
    return 0;
}
