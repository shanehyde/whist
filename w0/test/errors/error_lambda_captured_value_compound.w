// ERROR TEST: compound assignment to captured value type
// Expected error: Cannot assign to captured value type 'count' in lambda

func main() -> i32 {
    var count: i32 = 0;
    var inc = || -> void { count = count + 1; };
    return 0;
}
