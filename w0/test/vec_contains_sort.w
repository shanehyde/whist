// Test Vec contains() and sort()

func main(): i32 {
    var values = new Vec<i64>{3, 1, 2};

    if (!values.contains(2)) {
        return 1;
    }
    if (values.contains(99)) {
        return 2;
    }

    var empty = new Vec<i64>{};
    if (empty.contains(1)) {
        return 3;
    }

    values.sort();
    if (values[0] != 1) {
        return 4;
    }
    if (values[1] != 2) {
        return 5;
    }
    if (values[2] != 3) {
        return 6;
    }

    var already_sorted = new Vec<i64>{1, 2, 3};
    already_sorted.sort();
    if (already_sorted[0] != 1 || already_sorted[1] != 2 || already_sorted[2] != 3) {
        return 7;
    }

    var reverse = new Vec<i64>{5, 4, 3, 2, 1};
    reverse.sort();
    if (reverse[0] != 1 || reverse[1] != 2 || reverse[2] != 3 || reverse[3] != 4 ||
        reverse[4] != 5) {
        return 8;
    }

    return 0;
}
