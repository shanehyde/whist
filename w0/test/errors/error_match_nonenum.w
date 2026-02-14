// Expected error: Match expression must be an enum type

func main(): i32 {
    var x: i64 = 42;
    match (x) {
        _ => { return 0; },
    }
    return 0;
}
