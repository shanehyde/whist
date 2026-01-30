// Test pointers

func swap(a: *int32, b: *int32) {
    var temp = *a;
    *a = *b;
    *b = temp;
}

func main(): int32 {
    var x: int32 = 10;
    var y: int32 = 20;

    // Address-of and dereference
    var px: *int32 = &x;
    *px = 15;

    // Pass by pointer
    swap(&x, &y);

    return x + y;
}
