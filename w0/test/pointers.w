// Test pointers

func swap(a: *int, b: *int) {
    var temp = *a;
    *a = *b;
    *b = temp;
}

func main(): int {
    var x = 10;
    var y = 20;

    // Address-of and dereference
    var px: *int = &x;
    *px = 15;

    // Pass by pointer
    swap(&x, &y);

    return x + y;
}
