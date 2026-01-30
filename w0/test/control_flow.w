// Test control flow statements

func main(): i32 {
    var x = 0;

    // If-else
    if (x == 0) {
        x = 1;
    } else {
        x = 2;
    }

    // While loop
    while (x < 10) {
        x = x + 1;
    }

    // For loop
    for (var i = 0; i < 5; i++) {
        x = x + i;
    }

    // Break and continue
    for (var j = 0; j < 100; j++) {
        if (j == 50) {
            break;
        }
        if (j % 2 == 0) {
            continue;
        }
        x = x + 1;
    }

    return x;
}
