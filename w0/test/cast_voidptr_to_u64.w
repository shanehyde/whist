// Test voidptr -> u64 cast

func main(): i32 {
    var p: voidptr = null;
    var addr: u64 = p as u64;
    if (addr != 0) {
        return 1;
    }
    return 0;
}
