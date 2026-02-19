// ERROR TEST: Redefinition of type alias
// Expected error: Redefinition of type

type Foo = i64;
type Foo = string;

func main() -> i32 {
    return 0;
}
