# Standard Library: Math

Mathematical functions, constants, and numeric utilities.

## Overview

| Category | Description |
|----------|-------------|
| Constants | Pi, E, infinity, NaN |
| Basic | Abs, min, max, clamp |
| Rounding | Floor, ceil, round, trunc |
| Powers | Sqrt, pow, exp, log |
| Trigonometry | Sin, cos, tan, asin, acos, atan |
| Hyperbolic | Sinh, cosh, tanh |
| Random | Random number generation |
| BigInt | Arbitrary precision integers |
| Complex | Complex numbers |

## Constants

```whist
import math;

math.PI         // 3.14159265358979...
math.E          // 2.71828182845904...
math.TAU        // 6.28318530717958... (2π)
math.PHI        // 1.61803398874989... (golden ratio)

math.INF        // Positive infinity
math.NEG_INF    // Negative infinity
math.NAN        // Not a number

// Type-specific limits
i64.MIN         // -9223372036854775808
i64.MAX         // 9223372036854775807
f64.MIN         // Smallest positive f64
f64.MAX         // Largest f64
f64.EPSILON     // Smallest difference between f64s
```

## Basic Functions

```whist
import math;

// Absolute value
math.abs(-5)        // 5
math.abs(-3.14)     // 3.14

// Min/Max
math.min(3, 7)      // 3
math.max(3, 7)      // 7
math.min(1, 2, 3)   // 1 (variadic)

// Clamp
math.clamp(x, 0, 100)   // x bounded to [0, 100]

// Sign
math.sign(-5)       // -1
math.sign(0)        // 0
math.sign(5)        // 1
math.copysign(5, -1) // -5

// Division
math.div_euclid(7, 3)   // 2 (Euclidean division)
math.rem_euclid(7, 3)   // 1 (Euclidean remainder)
```

### API

```whist
// Generic over numeric types
func abs<T: Signed>(x: T) -> T;
func min<T: Ord>(a: T, b: T) -> T;
func max<T: Ord>(a: T, b: T) -> T;
func clamp<T: Ord>(x: T, lo: T, hi: T) -> T;

// Integer specific
func gcd(a: i64, b: i64) -> i64;
func lcm(a: i64, b: i64) -> i64;
func factorial(n: i64) -> i64;

// Float specific
func sign(x: f64) -> f64;
func copysign(magnitude: f64, sign: f64) -> f64;
```

## Rounding

```whist
import math;

math.floor(3.7)     // 3.0
math.ceil(3.2)      // 4.0
math.round(3.5)     // 4.0 (round half away from zero)
math.trunc(3.7)     // 3.0 (toward zero)

// To integer
math.floor_i64(3.7) // 3
math.ceil_i64(3.2)  // 4
math.round_i64(3.5) // 4

// Fractional part
math.fract(3.7)     // 0.7
```

### API

```whist
func floor(x: f64) -> f64;
func ceil(x: f64) -> f64;
func round(x: f64) -> f64;
func trunc(x: f64) -> f64;
func fract(x: f64) -> f64;

func floor_i64(x: f64) -> i64;
func ceil_i64(x: f64) -> i64;
func round_i64(x: f64) -> i64;
```

## Powers and Roots

```whist
import math;

// Square root
math.sqrt(16.0)     // 4.0
math.cbrt(27.0)     // 3.0 (cube root)

// Powers
math.pow(2.0, 10.0) // 1024.0
math.powi(2.0, 10)  // 1024.0 (integer exponent, faster)

// Exponential
math.exp(1.0)       // 2.718... (e^x)
math.exp2(10.0)     // 1024.0 (2^x)
math.expm1(x)       // e^x - 1 (more accurate for small x)

// Logarithms
math.ln(math.E)     // 1.0 (natural log)
math.log2(1024.0)   // 10.0
math.log10(1000.0)  // 3.0
math.log(100.0, 10.0) // 2.0 (arbitrary base)
math.ln1p(x)        // ln(1 + x) (more accurate for small x)

// Hypotenuse
math.hypot(3.0, 4.0) // 5.0 (sqrt(a² + b²))
```

### API

```whist
func sqrt(x: f64) -> f64;
func cbrt(x: f64) -> f64;
func pow(base: f64, exp: f64) -> f64;
func powi(base: f64, exp: i32) -> f64;

func exp(x: f64) -> f64;
func exp2(x: f64) -> f64;
func expm1(x: f64) -> f64;

func ln(x: f64) -> f64;
func log2(x: f64) -> f64;
func log10(x: f64) -> f64;
func log(x: f64, base: f64) -> f64;
func ln1p(x: f64) -> f64;

func hypot(a: f64, b: f64) -> f64;
```

## Trigonometry

```whist
import math;

// Basic trig (radians)
math.sin(math.PI / 2.0)  // 1.0
math.cos(0.0)            // 1.0
math.tan(math.PI / 4.0)  // 1.0

// Inverse trig
math.asin(1.0)           // π/2
math.acos(0.0)           // π/2
math.atan(1.0)           // π/4
math.atan2(y, x)         // angle of point (x, y)

// Degrees conversion
math.to_radians(180.0)   // π
math.to_degrees(math.PI) // 180.0

// Sincos (both at once, efficient)
var (s, c) = math.sincos(angle);
```

### API

```whist
func sin(x: f64) -> f64;
func cos(x: f64) -> f64;
func tan(x: f64) -> f64;

func asin(x: f64) -> f64;
func acos(x: f64) -> f64;
func atan(x: f64) -> f64;
func atan2(y: f64, x: f64) -> f64;

func sincos(x: f64) -> (f64, f64);

func to_radians(degrees: f64) -> f64;
func to_degrees(radians: f64) -> f64;
```

## Hyperbolic Functions

```whist
import math;

math.sinh(x)     // Hyperbolic sine
math.cosh(x)     // Hyperbolic cosine
math.tanh(x)     // Hyperbolic tangent

math.asinh(x)    // Inverse hyperbolic sine
math.acosh(x)    // Inverse hyperbolic cosine
math.atanh(x)    // Inverse hyperbolic tangent
```

## Float Classification

```whist
import math;

math.is_nan(x)       // Is Not-a-Number
math.is_infinite(x)  // Is ±infinity
math.is_finite(x)    // Not NaN or infinity
math.is_normal(x)    // Not zero, subnormal, NaN, or infinity
math.is_subnormal(x) // Is denormalized

// Also as methods on f64
x.is_nan()
x.is_finite()
```

## Random Numbers

```whist
import random;

// Random values
random.random()           // f64 in [0, 1)
random.random_i64()       // Random i64
random.random_range(1, 100) // i64 in [1, 100)

// With seed (reproducible)
var rng = random.Rng::seed(12345);
rng.next_f64()           // 0.123...
rng.next_i64()           // ...
rng.next_range(1, 100)   // ...

// Collections
random.shuffle(array)     // Shuffle in place
random.choice(array)      // Random element
random.sample(array, 5)   // 5 random elements (no replacement)
```

### API

```whist
// Global RNG (thread-local, auto-seeded)
func random() -> f64;
func random_i64() -> i64;
func random_range(lo: i64, hi: i64) -> i64;
func random_bool() -> bool;
func shuffle<T>(items: Span<T>) -> void;
func choice<T>(items: Span<T>) -> ?T;
func sample<T>(items: Span<T>, n: i64) -> Vec<T>;

// Seeded RNG
struct Rng { ... }

impl Rng {
    func seed(s: u64) -> Rng;
    func from_entropy() -> Rng;

    func (Rng) next_u64() -> u64;
    func (Rng) next_i64() -> i64;
    func (Rng) next_f64() -> f64;
    func (Rng) next_range(lo: i64, hi: i64) -> i64;
    func (Rng) next_bool() -> bool;
}

// Distributions (optional)
struct Uniform { lo: f64, hi: f64 }
struct Normal { mean: f64, std_dev: f64 }
struct Bernoulli { p: f64 }
```

## Big Integers

Arbitrary precision integers:

```whist
import math.bigint;

var a = BigInt::from(i64.MAX);
var b = BigInt::from(i64.MAX);
var c = a * b;  // No overflow!

print(c);  // 85070591730234615847396907784232501249

var huge = BigInt::parse("123456789012345678901234567890")?;
var factorial = bigint.factorial(100);

// Operations
a + b
a - b
a * b
a / b
a % b
a.pow(100)
a.gcd(b)
a.abs()
a.is_negative()
```

### API

```whist
struct BigInt { ... }

impl BigInt {
    func from(n: i64) -> BigInt;
    func parse(s: string) -> Result<BigInt, ParseError>;

    func (BigInt) to_i64() -> ?i64;  // None if too big
    func (BigInt) to_string() -> string;

    // Arithmetic (operators also available)
    func (BigInt) add(other: BigInt) -> BigInt;
    func (BigInt) sub(other: BigInt) -> BigInt;
    func (BigInt) mul(other: BigInt) -> BigInt;
    func (BigInt) div(other: BigInt) -> BigInt;
    func (BigInt) rem(other: BigInt) -> BigInt;
    func (BigInt) pow(exp: u32) -> BigInt;

    // Comparison
    func (BigInt) cmp(other: BigInt) -> Ordering;
    func (BigInt) abs() -> BigInt;
    func (BigInt) is_negative() -> bool;
    func (BigInt) is_zero() -> bool;

    // Number theory
    func (BigInt) gcd(other: BigInt) -> BigInt;
    func (BigInt) lcm(other: BigInt) -> BigInt;
    func (BigInt) mod_pow(exp: BigInt, modulus: BigInt) -> BigInt;
}
```

## Complex Numbers

```whist
import math.complex;

var z1 = Complex { re: 3.0, im: 4.0 };
var z2 = Complex::from_polar(5.0, 0.927);  // r, theta

// Arithmetic
var sum = z1 + z2;
var product = z1 * z2;
var quotient = z1 / z2;

// Properties
z1.abs()      // Magnitude: 5.0
z1.arg()      // Argument (angle) -> 0.927...
z1.conj()     // Conjugate: 3 - 4i
z1.norm()     // |z|²: 25.0

// Functions
z1.sqrt()
z1.exp()
z1.ln()
z1.sin()
z1.cos()
```

### API

```whist
struct Complex {
    re: f64,
    im: f64,
}

impl Complex {
    func new(re: f64, im: f64) -> Complex;
    func from_polar(r: f64, theta: f64) -> Complex;

    func (Complex) abs() -> f64;
    func (Complex) arg() -> f64;
    func (Complex) norm() -> f64;
    func (Complex) conj() -> Complex;

    func (Complex) sqrt() -> Complex;
    func (Complex) exp() -> Complex;
    func (Complex) ln() -> Complex;
    func (Complex) pow(exp: Complex) -> Complex;

    func (Complex) sin() -> Complex;
    func (Complex) cos() -> Complex;
    func (Complex) tan() -> Complex;
}

const I: Complex = Complex { re: 0.0, im: 1.0 };
```

## Vectors and Matrices (Optional)

For numerical computing:

```whist
import math.linalg;

// Vectors
var v1 = Vec3 { x: 1.0, y: 2.0, z: 3.0 };
var v2 = Vec3 { x: 4.0, y: 5.0, z: 6.0 };

v1.dot(v2)      // Dot product
v1.cross(v2)    // Cross product
v1.magnitude()  // Length
v1.normalize()  // Unit vector

// Matrices
var m = Mat4::identity();
var rotated = Mat4::rotation_y(angle) * m;
var transformed = m.transform_point(point);
```

## Examples

### Quadratic Formula

```whist
import math;

func solve_quadratic(a: f64, b: f64, c: f64) -> ?((f64, f64)) {
    var discriminant = b * b - 4.0 * a * c;

    if discriminant < 0.0 {
        return None;  // No real solutions
    }

    var sqrt_d = math.sqrt(discriminant);
    var x1 = (-b + sqrt_d) / (2.0 * a);
    var x2 = (-b - sqrt_d) / (2.0 * a);

    return Some((x1, x2));
}
```

### Distance Between Points

```whist
import math;

struct Point { x: f64, y: f64 }

func distance(p1: Point, p2: Point) -> f64 {
    var dx = p2.x - p1.x;
    var dy = p2.y - p1.y;
    return math.hypot(dx, dy);
}
```

### Monte Carlo Pi

```whist
import random;

func estimate_pi(samples: i64) -> f64 {
    var inside = 0;

    foreach _ in 0..samples {
        var x = random.random();
        var y = random.random();
        if x * x + y * y <= 1.0 {
            inside += 1;
        }
    }

    return 4.0 * (inside as f64) / (samples as f64);
}
```

### Prime Check

```whist
import math;

func is_prime(n: i64) -> bool {
    if n < 2 { return false; }
    if n == 2 { return true; }
    if n % 2 == 0 { return false; }

    var limit = math.sqrt(n as f64) as i64 + 1;
    var i = 3;
    while i <= limit {
        if n % i == 0 { return false; }
        i += 2;
    }

    return true;
}
```

## Open Questions

1. **SIMD support?**
   - Expose SIMD intrinsics
   - Auto-vectorization only

2. **Linear algebra scope?**
   - Basic vectors/matrices built-in
   - Or separate library

3. **Arbitrary precision?**
   - BigInt built-in
   - BigDecimal too?

4. **Random algorithms?**
   - Which PRNG (xorshift, PCG, etc.)
   - Cryptographic random available?

## Related Features

- [Generics](../FEATURES.md) - Generic math functions
- [Traits](traits.md) - Numeric traits (Add, Mul, etc.)
- [Union Types](union-types.md) - Optional results
