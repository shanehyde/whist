// Test various integer types

func main(): i32 {
    // Signed integers
    var a: i8 = 127;
    var b: i16 = 32767;
    var c: i32 = 2147483647;
    var d: i32 = 1234567890;

    // Unsigned integers
    var e: u8 = 255;
    var f: u16 = 65535;
    var g: u32 = 4294967295;

    // Arithmetic operations
    var sum: i32 = c + 1;
    var diff: i16 = b - 100;

    // Bitwise operations
    var masked: u8 = e & 0x0F;
    var shifted: u16 = f >> 8;

    // Mixed operations (result is i64)
    var mixed = a + b;

    return 0;
}
