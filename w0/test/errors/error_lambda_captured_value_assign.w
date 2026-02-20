// ERROR TEST: assigning to captured value types in lambdas
// Expected error: Cannot assign to captured value type 'total' in lambda

func main() -> i32 {
    var total = 0;
    var adder = |x: i64| -> void { total += x; };
    return 0;
}
