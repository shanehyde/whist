// Test operators

func main(): i32 {
    var a = 10;
    var b = 3;

    // Arithmetic
    var sum = a + b;
    var diff = a - b;
    var prod = a * b;
    var quot = a / b;
    var rem = a % b;

    // Comparison
    var eq = a == b;
    var ne = a != b;
    var lt = a < b;
    var gt = a > b;
    var le = a <= b;
    var ge = a >= b;

    // Logical
    var and = eq && ne;
    var or = lt || gt;
    var not = !eq;

    // Bitwise
    var band = a & b;
    var bor = a | b;
    var bxor = a ^ b;
    var bnot = ~a;
    var shl = a << 2;
    var shr = a >> 1;

    // Compound assignment
    a += 5;
    a -= 2;
    a *= 3;
    a /= 2;

    return a;
}
