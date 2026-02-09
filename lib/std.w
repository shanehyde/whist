// Standard library for Whist

private extern stdio {
    func printf(fmt: string, ...): i32;
}

private extern stdlib {
    func exit(status: i32) as _exit;
}

func print(s: string): void {
    printf("%s", s);
}

func println(s: string): void {
    printf("%s\n", s);
}

func exit(status: i32): void {
    _exit(status);
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

// String/integer conversion
private extern std_str {
    func std__parse_i64(s: string): i64;
    func std__to_string(n: i64): string;
}

func parse_i64(s: string): i64 {
    return std__parse_i64(s);
}

func to_string(n: i64): string {
    return std__to_string(n);
}

// Private internal function - should not be visible outside std
private func _internal_std(): i64 {
    return 42;
}
