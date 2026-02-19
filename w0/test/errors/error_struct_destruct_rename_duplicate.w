// Test: duplicate local names in struct destructuring with rename

struct Point {
    x: i64,
    y: i64,
}

func main() -> i32 {
    var {x: a, y: a} = new Point {x: 1, y: 2};
    // Expected error: Redefinition of 'a'
    return 0;
}
