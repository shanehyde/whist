// Test various integer types

func main(): int {
    // Signed integers
    var a: int8 = 127;
    var b: int16 = 32767;
    var c: int32 = 2147483647;
    var d: int = 9223372036854775807;

    // Unsigned integers
    var e: uint8 = 255;
    var f: uint16 = 65535;
    var g: uint32 = 4294967295;

    // Arithmetic operations
    var sum: int32 = c + 1;
    var diff: int16 = b - 100;

    // Bitwise operations
    var masked: uint8 = e & 0x0F;
    var shifted: uint16 = f >> 8;

    // Mixed operations (result is int)
    var mixed = a + b;

    return 0;
}
