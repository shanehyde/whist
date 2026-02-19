// Expected error: if-let requires an enum type, got 'i64'

func main() -> i32 {
    var x: i64 = 42;
    if let Some(v) = x {
        return 0;
    }
    return 0;
}
