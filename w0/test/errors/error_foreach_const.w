// ERROR TEST: Assignment to foreach const variable
// Expected error: Cannot assign to const 'i'

func main() -> i32 {
    foreach (const i in 0..5) {
        i = 10;  // Error: cannot assign to const variable
    }
    return 0;
}
