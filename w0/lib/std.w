// Standard library for Whist

extern stdio {
    func printf(fmt:string, s: string): void;
}

func print(s: string): void {
    printf("%s", s);
}

func abs_i64(x: i64): i64 {
    if (x < 0) {
        return -x;
    }
    return x;
}

func max_i64(a: i64, b: i64): i64 {
    if (a > b) {
        return a;
    }
    return b;
}

func min_i64(a: i64, b: i64): i64 {
    if (a < b) {
        return a;
    }
    return b;
}

// Private internal function - should not be visible outside std
private func _internal_std(): i64 {
    return 42;
}
