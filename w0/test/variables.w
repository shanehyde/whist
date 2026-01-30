// Test variable declarations and types

func main(): int32 {
    // Type inference
    var a = 42;
    var b = 3.14;
    var c = true;
    var d = "hello";
    var e = 'x';

    // Explicit types
    var x: int32 = 100;
    var y: float = 2.718;
    var z: bool = false;

    // Constants
    const PI = 3.14159;
    const MAX = 1000;

    // Numeric literals
    var hex = 0xFF;
    var binary = 0b1010;
    var octal = 0o755;
    var scientific = 1.5e10;

    return a + x;
}
