// ERROR TEST: Duplicate names in destructuring pattern
// Expected error: Redefinition of 'a'

func main(): i32 {
    var (a, a) = (1, 2);
    return a;
}
