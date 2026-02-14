// Test nested Vec<Vec<i64>> — create, push, index, count

func main(): i32 {
    var outer = new Vec<Vec<i64>>{};

    var inner1 = new Vec<i64>{1, 2, 3};
    var inner2 = new Vec<i64>{4, 5};

    outer.push(inner1);
    outer.push(inner2);

    if (outer.count != 2) {
        return 1;
    }

    if (outer[0].count != 3) {
        return 2;
    }

    if (outer[1].count != 2) {
        return 3;
    }

    if (outer[0][0] != 1) {
        return 4;
    }

    if (outer[0][2] != 3) {
        return 5;
    }

    if (outer[1][1] != 5) {
        return 6;
    }

    // Push to inner vec after it's in outer
    outer[0].push(10);

    if (outer[0].count != 4) {
        return 7;
    }

    if (outer[0][3] != 10) {
        return 8;
    }

    // outer goes out of scope — should __rc_dec each inner Vec
    return 0;
}
