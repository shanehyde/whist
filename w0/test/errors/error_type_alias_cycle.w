// ERROR TEST: Recursive type alias (self-reference)
// Expected error: Unknown type 'A'

type A = A;

func main() -> i32 {
    return 0;
}
