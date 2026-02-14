// ERROR TEST: assert() requires exactly 1 argument
// Expected error: assert() requires exactly 1 argument

test "bad assert" {
    assert(true, false);
}

func main(): i32 {
    return 0;
}
