// Test function definitions and calls

func add(a: i32, b: i32): i32 {
    return a + b;
}

func multiply(x: i32, y: i32): i32 {
    return x * y;
}

func factorial(n: i32): i32 {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

func main(): i32 {
    var sum = add(10, 20);
    var product = multiply(5, 6);
    var fact = factorial(5);
    return sum + product + fact;
}
