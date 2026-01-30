// Test shift assignment operators <<= and >>=

func main(): i32 {
    var a: i32 = 1;
    var b: i32 = 256;

    // Left shift assignment
    a <<= 4;  // a = 1 << 4 = 16

    // Right shift assignment
    b >>= 4;  // b = 256 >> 4 = 16

    // Verify both are equal
    if (a == b && a == 16) {
        return 0;
    }
    return 1;
}
