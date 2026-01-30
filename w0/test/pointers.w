// Test pointers

func swap(a: *i32, b: *i32) {
    var temp = *a;
    *a = *b;
    *b = temp;
}

func main(): i32 {
    var x: i32 = 10;
    var y: i32 = 20;

    // Address-of and dereference
    var px: *i32 = &x;
    *px = 15;

    // Pass by pointer
    swap(&x, &y);

    return x + y;
}
