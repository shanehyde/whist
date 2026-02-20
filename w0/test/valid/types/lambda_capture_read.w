// Valid: reading captured value types is allowed (copy is correct at capture time)

func main() -> i32 {
    var x = 42;
    var reader = || -> i64 { return x; };
    return 0;
}
