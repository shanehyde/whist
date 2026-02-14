// ERROR TEST: assert() argument must be bool
// Expected error: assert() argument must be bool, got 'i64'

test "bad assert type" {
    assert(42);
}

func main(): i32 {
    return 0;
}
