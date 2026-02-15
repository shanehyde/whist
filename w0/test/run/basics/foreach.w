// Expected: PASS: foreach_empty
// Expected: PASS: foreach_range
// Expected: PASS: foreach_simple
// Expected: PASS: foreach_span
// Expected: PASS: foreach_string
// Expected: PASS: foreach_vec_empty
// Expected: PASS: foreach_vec
// Expected: PASS: foreach_vec_struct

// Shared type definitions
struct Point { x: i64, y: i64 }

// Test foreach over empty range
test "foreach_empty" {
    var count = 0;
    foreach (const i in 5..5) {  // Exclusive: 5..5 is empty range
        count = count + 1;
    }
    assert(count == 0);
}

// Test foreach over range
test "foreach_range" {
    var total = 0;
    foreach (const i in 1..6) {
        total = total + i;
    }
    assert(total == 15);
}

// Test foreach with step
test "foreach_simple" {
    var result = 0;
    var z = 2;

    foreach (const num in 10..14 by z) {
        result = result + num;
    }
    assert(result == 22);  // 10 + 12 = 22 (14 excluded)
}

// Test foreach over Span<i64>
test "foreach_span" {
    var arr: [5]i64;
    arr[0] = 10;
    arr[1] = 20;
    arr[2] = 30;
    arr[3] = 40;
    arr[4] = 50;

    var s: Span<i64> = arr[:];

    var total: i64 = 0;
    foreach (const n in s) {
        total = total + n;
    }

    assert(total == 150);

    // Foreach over a slice of the span
    var s2: Span<i64> = arr[1:4];
    var total2: i64 = 0;
    foreach (const n in s2) {
        total2 = total2 + n;
    }

    assert(total2 == 90);

    // Foreach over empty span
    var empty: Span<i64> = arr[0:0];
    var total3: i64 = 0;
    foreach (const n in empty) {
        total3 = total3 + n;
    }

    assert(total3 == 0);
}

// Test foreach over string (yields char)
test "foreach_string" {
    var s: string = "abc";
    var count: i64 = 0;

    foreach (const c in s) {
        if (count == 0) {
            assert(c == 'a');
        }
        if (count == 1) {
            assert(c == 'b');
        }
        if (count == 2) {
            assert(c == 'c');
        }
        count = count + 1;
    }

    assert(count == 3);

    // Test empty string
    var empty: string = "";
    var empty_count: i64 = 0;
    foreach (const c in empty) {
        empty_count = empty_count + 1;
    }
    assert(empty_count == 0);
}

// Test foreach over empty Vec iterates zero times
test "foreach_vec_empty" {
    var nums = new Vec<i64>{};

    var count: i64 = 0;
    foreach (const n in nums) {
        count = count + 1;
    }

    assert(count == 0);
}

// Test foreach over Vec<i64>
test "foreach_vec" {
    var nums = new Vec<i64>{};

    nums.push(10);
    nums.push(20);
    nums.push(30);

    var total: i64 = 0;
    foreach (const n in nums) {
        total = total + n;
    }

    assert(total == 60);
}

// Test foreach over Vec<Point> with struct field access
test "foreach_vec_struct" {
    var points = new Vec<Point>{};

    points.push(new Point{x: 1, y: 2});
    points.push(new Point{x: 3, y: 4});
    points.push(new Point{x: 5, y: 6});

    var sum_x: i64 = 0;
    var sum_y: i64 = 0;
    foreach (const p in points) {
        sum_x = sum_x + p.x;
        sum_y = sum_y + p.y;
    }

    assert(sum_x == 9);
    assert(sum_y == 12);
}
