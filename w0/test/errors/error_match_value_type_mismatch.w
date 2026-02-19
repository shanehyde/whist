// Expected error: Pattern type 'string' is not compatible with match expression type 'i64'

func main() -> i32 {
    var x: i64 = 42;
    match (x) {
        "hello" => { return 1; },
        _ => { return 0; },
    }
    return 0;
}
