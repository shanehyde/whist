// ERROR TEST: Cannot call mutating method on const StringBuilder
// Expected error: Cannot call mutating method 'append' on const 'sb'

func main(): i32 {
    const sb = new StringBuilder{};
    sb.append("x");
    return 0;
}
