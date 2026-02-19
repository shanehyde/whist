// Expected error: 'is' pattern requires an enum type, got 'i64'

func main() -> i32 {
    var x: i64 = 42;
    if (x is Some(v)) {
        return 0;
    }
    return 0;
}
