// Expected error: Match expression must be an enum, integer, float, string, char, or bool type

struct Point { x: i64, y: i64 }

func main() -> i32 {
    var p = new Point { x: 1, y: 2 };
    match (p) {
        _ => { return 0; },
    }
    return 0;
}
