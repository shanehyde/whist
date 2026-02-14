// Expected: PASS: cast_voidptr_to_u64
// Test voidptr -> u64 cast

test "cast_voidptr_to_u64" {
    var p: voidptr = null;
    var addr: u64 = p as u64;
    assert(addr == 0);
}
