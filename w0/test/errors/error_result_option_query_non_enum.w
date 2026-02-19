// Expected error: Member access requires struct type, got 'i64'

func main() -> i32 {
    var x: i64 = 42;
    var y = x.has_value();
    return 0;
}
