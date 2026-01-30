// Test function definitions and calls

func add(a: int, b: int): int {
    return a + b;
}

func multiply(x: int, y: int): int {
    return x * y;
}

func factorial(n: int): int {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

func main(): int {
    var sum = add(10, 20);
    var product = multiply(5, 6);
    var fact = factorial(5);
    return sum + product + fact;
}
