// Expected error: Duplicate match pattern

func main() -> i32 {
    var x: i64 = 42;
    match (x) {
        1 => { return 1; },
        1 => { return 2; },
        _ => { return 0; },
    }
    return 0;
}
