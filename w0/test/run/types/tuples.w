// Test tuple types and operations
// Expected exit: 89

func main(): i32 {
    // Basic tuple literal with type annotation
    var t: (i64, string) = (42, "hello");

    // Tuple element access
    var x: i64 = t[0];

    // Type-inferred tuple
    var pair = (1, 2);

    // Destructuring
    var (a, b) = (10, 20);

    // Nested destructuring
    var (p, (q, r)) = (3, (4, 5));

    // Nested tuples
    var nested: (i64, (i64, i64)) = (1, (2, 3));
    var inner: (i64, i64) = nested[1];
    var innerFirst: i64 = inner[0];

    // Return sum to verify values
    return x + pair[0] + pair[1] + a + b + p + q + r + innerFirst;
}
