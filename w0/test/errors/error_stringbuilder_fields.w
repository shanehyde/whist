// ERROR TEST: StringBuilder does not accept field initializers
// Expected error: StringBuilder does not accept field initializers

func main() -> i32 {
    var sb = new StringBuilder{field: 1};
    return 0;
}
