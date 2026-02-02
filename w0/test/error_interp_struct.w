// Error: Cannot interpolate struct type
// Expected error: Cannot interpolate type 'Point'

struct Point { x: i64, y: i64 }

func main(): i32 {
    var p: Point = { x: 1, y: 2 };
    var msg = `Point: ${p}`;  // ERROR: structs cannot be interpolated
    return 0;
}
