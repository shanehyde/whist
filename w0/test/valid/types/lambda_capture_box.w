// Valid: assigning to a captured Box is allowed (shared mutable reference)

func main() -> i32 {
    var ^total = 0;
    var adder = |x: i64| -> void { total += x; };
    return 0;
}
