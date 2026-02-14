// Expected: PASS: shift_assign
// Test shift assignment operators <<= and >>=

test "shift_assign" {
    var a: i32 = 1;
    var b: i32 = 256;

    // Left shift assignment
    a <<= 4;  // a = 1 << 4 = 16

    // Right shift assignment
    b >>= 4;  // b = 256 >> 4 = 16

    // Verify both are equal
    assert(a == b);
    assert(a == 16);
}
