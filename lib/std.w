// Standard library for Whist
// Note: Result<T, E> is now provided by the prelude (auto-imported).

private extern stdio {
    func printf(fmt: string, ...) -> i32;
}

private extern stdlib {
    func exit(status: i32) -> void as _exit;
    func system(cmd: string) -> i32 as _system;
}

private extern std_args {
    func std__argc() -> i64;
    func std__argv(i: i64) -> string;
}

private extern std_io {
    func std__eprint(s: string) -> void;
    func std__eprintln(s: string) -> void;
}

public func print(s: string) -> void {
    printf("%s", s);
}

public func println(s: string) -> void {
    printf("%s\n", s);
}

public func eprint(s: string) -> void {
    std__eprint(s);
}

public func eprintln(s: string) -> void {
    std__eprintln(s);
}

public func exit(status: i32) -> void {
    _exit(status);
}

public func system(cmd: string) -> i32 {
    return _system(cmd);
}

public func args() -> Vec<string> {
    var args = new Vec<string>{};

    var count = std__argc();
    var i: i64 = 0;
    while (i < count) {
        args.push(std__argv(i));
        i = i + 1;
    }

    return args;
}

public func panic(s: string) -> void {
    println($"Panic: {s}");
    exit(1);
}

public func abs_i64(x: i64) -> i64 {
    if (x < 0) {
        return -x;
    }
    return x;
}

public func max_i64(a: i64, b: i64) -> i64 {
    if (a > b) {
        return a;
    }
    return b;
}

public func min_i64(a: i64, b: i64) -> i64 {
    if (a < b) {
        return a;
    }
    return b;
}

// String/integer conversion
private extern std_str {
    func std__parse_i64(s: string) -> i64;
    func std__to_string(n: i64) -> string;
}

public func parse_i64(s: string) -> i64 {
    return std__parse_i64(s);
}

public func to_string(n: i64) -> string {
    return std__to_string(n);
}

// Command execution with output capture
public struct ExecResult {
    exit_code: i32,
    output: string,
    error_output: string
}

private extern std_exec {
    func std__exec(cmd: string) -> voidptr;
    func std__exec_exit_code(handle: voidptr) -> i32;
    func std__exec_output(handle: voidptr) -> string;
    func std__exec_error_output(handle: voidptr) -> string;
    func std__exec_free(handle: voidptr) -> void;
}

public func exec(cmd: string) -> ExecResult {
    var handle = std__exec(cmd);
    var result = new ExecResult {
        exit_code: std__exec_exit_code(handle),
        output: std__exec_output(handle),
        error_output: std__exec_error_output(handle)
    };
    std__exec_free(handle);
    return result;
}

// Private internal function - should not be visible outside std
private func _internal_std() -> i64 {
    return 42;
}
