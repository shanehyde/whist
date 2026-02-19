// ERROR TEST: Invalid cast from voidptr to struct reference
// Expected error: Cannot cast 'voidptr' to 'Box'

struct Box {
    value: i64,
}

func main() -> i32 {
    var p: voidptr = null;
    var b: Box = p as Box;
    return b.value as i32;
}
