// Struct initializer must initialize every field
// Expected error: Missing initializer for field 'y'

struct Point {
    x: i32,
    y: i32,
}

func main(): i32 {
    var p = new Point {x: 10};
    return p.x;
}
