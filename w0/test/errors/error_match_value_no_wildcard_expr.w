// Expected error: Match expression requires a wildcard '_' arm

func main() -> i32 {
    var x: i64 = 42;
    var result = match(x) {
        1 => 10,
        2 => 20,
    };
    return 0;
}
