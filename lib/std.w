// Standard library for Whist
// Note: Result<T, E> is now provided by the prelude (auto-imported).

private extern stdio {
    func printf(fmt: string, ...): i32;
}

private extern stdlib {
    func exit(status: i32): void as _exit;
    func system(cmd: string): i32 as _system;
}

private extern std_args {
    func std__argc(): i64;
    func std__argv(i: i64): string;
}

private extern std_io {
    func std__eprint(s: string): void;
    func std__eprintln(s: string): void;
}

func print(s: string): void {
    printf("%s", s);
}

func println(s: string): void {
    printf("%s\n", s);
}

func eprint(s: string): void {
    std__eprint(s);
}

func eprintln(s: string): void {
    std__eprintln(s);
}

func exit(status: i32): void {
    _exit(status);
}

func system(cmd: string): i32 {
    return _system(cmd);
}

func args(): Vec<string> {
    var args = new Vec<string>{};

    var count = std__argc();
    var i: i64 = 0;
    while (i < count) {
        args.push(std__argv(i));
        i = i + 1;
    }

    return args;
}

func panic(s: string): void {
    println($"Panic: {s}");
    exit(1);
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
