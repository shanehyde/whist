// Whist Test Runner
// Replaces scripts/run_tests.sh — dogfooding the compiler.

import std;
import fs;
import time;
import collections;

struct Timer {
    start: i64,
    desc: string,
}

impl Timer {
    func init(desc: string) {
        self.start = time::time_ms();
        self.desc = desc;
    }
}

func (Timer) timelapsed() -> i64 {
    return time::time_ms() - self.start;
}

impl Drop for Timer {
    func drop() {
        var elapsed = self.timelapsed();
        std::println($"{self.desc} took {elapsed} ms");
    }
}

// --- ANSI colors ---

func red(s: string) -> string {
    return $"\e[1;31m{s}\e[0m";
}
func green(s: string) -> string {
    return $"\e[1;32m{s}\e[0m";
}
func blue(s: string) -> string {
    return $"\e[1;34m{s}\e[0m";
}
func cyan(s: string) -> string {
    return $"\e[1;36m{s}\e[0m";
}
func yellow(s: string) -> string {
    return $"\e[1;33m{s}\e[0m";
}
func gray(s: string) -> string {
    return $"\e[90m{s}\e[0m";
}

func ansi(code: string) -> string {
    return $"\e[{code}m";
}

// --- RC helpers ---

func extract_rc_addresses(output: string, prefix: string) -> Vec<string> {
    var addrs = new Vec<string>{};
    output.split("\n").filter(|line| line.starts_with(prefix)).each(|line| {
        var parts = line.split(" ");
        if (parts.count > 1) {
            addrs.push(parts[1]);
        }
    });
    addrs.sort();
    return addrs;
}

func check_rc_leaks(stderr: string) -> bool {
    var allocs = extract_rc_addresses(stderr, "RC_ALLOC:");
    var frees = extract_rc_addresses(stderr, "RC_FREE:");
    if (allocs.is_empty()) {
        return true;
    }
    var free_set = new Set<string>(std::max_i64(64, frees.count * 2));
    free_set.insert_all(frees);
    foreach (const addr in allocs) {
        if (!free_set.contains(addr)) {
            return false;
        }
    }
    return true;
}

func get_leaked_addresses(stderr: string) -> Vec<string> {
    var allocs = extract_rc_addresses(stderr, "RC_ALLOC:");
    var frees = extract_rc_addresses(stderr, "RC_FREE:");
    var free_set = new Set<string>(std::max_i64(64, frees.count * 2));
    free_set.insert_all(frees);
    var leaked = new Vec<string>{};
    foreach (const addr in allocs) {
        if (!free_set.contains(addr)) {
            leaked.push(addr);
        }
    }
    return leaked;
}

func check_rc_free_order(stderr: string, expected: string) -> bool {
    var parts = expected.split(" before ");
    if (parts.count != 2) {
        return false;
    }
    var left_idx = std::parse_i64(parts[0]);
    var right_idx = std::parse_i64(parts[1]);

    // Get alloc addresses in order of allocation
    var alloc_addrs = new Vec<string>{};
    foreach (const line in stderr.split("\n")) {
        if (line.starts_with("RC_ALLOC:")) {
            var line_parts = line.split(" ");
            if (line_parts.count > 1) {
                alloc_addrs.push(line_parts[1]);
            }
        }
    }

    if (left_idx < 1 || left_idx > alloc_addrs.count || right_idx < 1 || right_idx > alloc_addrs.count) {
        return false;
    }

    var left_ptr = alloc_addrs[left_idx - 1];
    var right_ptr = alloc_addrs[right_idx - 1];

    // Find positions of frees in order
    var left_pos: i64 = -1;
    var right_pos: i64 = -1;
    var pos: i64 = 0;
    foreach (const line in stderr.split("\n")) {
        if (line.starts_with("RC_FREE:")) {
            var line_parts = line.split(" ");
            if (line_parts.count > 1) {
                if (line_parts[1] == left_ptr && left_pos == -1) {
                    left_pos = pos;
                }
                if (line_parts[1] == right_ptr && right_pos == -1) {
                    right_pos = pos;
                }
            }
            pos += 1;
        }
    }

    if (left_pos == -1 || right_pos == -1) {
        return false;
    }
    return left_pos <= right_pos;
}

func filter_rc_lines(output: string) -> string {
    var sb = new StringBuilder{};
    var first = true;
    foreach (const line in output.split("\n")) {
        if (!line.starts_with("RC_ALLOC:") && !line.starts_with("RC_FREE:") && !line.starts_with("RC_INC:") && !line.starts_with("RC_DEC:")) {
            if (!first) {
                sb.append("\n");
            }
            sb.append(line);
            first = false;
        }
    }
    return sb.to_string();
}

// --- File/output helpers ---

func read_expected_lines(path: string, prefix: string) -> Vec<string> {
    // var lines = new Vec<string>{};
    var marker = $"// {prefix}: ";

    var lines = fs::read_file(path)
        .split("\n")
        .filter(|line| line.starts_with(marker))
        .map(|line| line.strip_prefix(marker));

    // foreach (const line in fs::read_file(path).split("\n")) {
    //     if (line.starts_with(marker)) {
    //         lines.push(line.strip_prefix(marker));
    //     }
    // }
    return lines;
}

func file_contains(path: string, pattern: string) -> bool {
    var content = fs::read_file(path);
    return content.contains(pattern);
}

func display_path(file: string) -> string {
    return file.strip_prefix("test/");
}

func extract_error_message(output: string) -> string {
    foreach (const line in output.split("\n")) {
        var idx = line.index_of("Error:");
        if (idx >= 0) {
            return line[idx + 6:].trim_start();
        }
    }
    return "";
}

// --- Test runners ---

func run_program_test(file: string, w0: string, lib_path: string, verbose: bool) -> Result<bool, string> {
    var tmp_bin = "/tmp/whist_test_bin";

    // Step 1: Compile with --rc-debug
    var compile_cmd = $"{w0} --rc-debug --lib-path {lib_path} {file} | cc -x c -I{lib_path}/include -o {tmp_bin} - {lib_path}/whist_runtime.c";
    var compile_result = std::exec(compile_cmd);
    if (compile_result.exit_code != 0) {
        if (verbose) {
            std::println($"  command: {compile_cmd}");
            std::println($"  output:  {compile_result.error_output}");
        }
        return Result::Err("compile");
    }

    // Step 2: Run the compiled binary
    var run_result = std::exec(tmp_bin);
    fs::remove_file(tmp_bin);

    // Check expected exit code
    var expected_exit_lines = read_expected_lines(file, "Expected exit");
    var expected_exit: i64 = 0;
    if (!expected_exit_lines.is_empty()) {
        expected_exit = std::parse_i64(expected_exit_lines[0]);
    }

    var actual_exit: i64 = run_result.exit_code as i64;
    if (actual_exit != expected_exit) {
        if (verbose) {
            std::println($"  exit code: {actual_exit} (expected {expected_exit})");
            std::println($"  stdout: {run_result.output}");
            std::println($"  stderr: {run_result.error_output}");
        }
        return Result::Err($"exit {actual_exit}, expected {expected_exit}");
    }

    // Check expected stdout
    var expected_stdout = read_expected_lines(file, "Expected");
    if (!expected_stdout.is_empty()) {
        if (!expected_stdout.all(|line| run_result.output.contains(line))) {
            if (verbose) {
                std::println("  stdout mismatch:");
                std::println($"  actual: {run_result.output}");
                foreach (const line in expected_stdout) {
                    std::println($"  expected line: {line}");
                }
            }
            return Result::Err("stdout");
        }
    }

    // Check expected stderr (filter RC debug lines first)
    var expected_stderr = read_expected_lines(file, "Expected stderr");
    if (!expected_stderr.is_empty()) {
        var filtered_stderr = filter_rc_lines(run_result.error_output);
        if (!expected_stderr.all(|line| filtered_stderr.contains(line))) {
            if (verbose) {
                std::println("  stderr mismatch:");
                std::println($"  actual: {filtered_stderr}");
            }
            return Result::Err("stderr");
        }
    }

    // Check expected RC free order
    var expected_order_lines = read_expected_lines(file, "Expected rc free order");
    if (!expected_order_lines.is_empty()) {
        if (!check_rc_free_order(run_result.error_output, expected_order_lines[0])) {
            if (verbose) {
                std::println("  rc free order mismatch:");
                std::println($"  expected: {expected_order_lines[0]}");
            }
            return Result::Err("rc free order");
        }
    }

    // Check RC leaks (only on clean exits)
    if (expected_exit == 0) {
        if (!check_rc_leaks(run_result.error_output)) {
            if (verbose) {
                std::println("  rc leak detected:");
                var leaked = get_leaked_addresses(run_result.error_output);
                foreach (const addr in leaked) {
                    std::println($"  leaked: {addr}");
                }
            }
            return Result::Err("rc leak");
        }
    }

    return Result::Ok(true);
}

func run_test_block_file(file: string, w0: string, lib_path: string, verbose: bool) -> Result<bool, string> {
    var cmd = $"{w0} --rc-debug --lib-path {lib_path} test {file}";
    var {output, error_output, exit_code} = std::exec(cmd);

    // Build combined output with RC lines filtered from stderr
    var combined = $"{output}{filter_rc_lines(error_output)}";

    // Check expected output lines
    var expected_lines = read_expected_lines(file, "Expected");

    if (exit_code != 0 && expected_lines.is_empty()) {
        if (verbose) {
            std::println($"  command: {cmd}");
            std::println($"  output:  {combined}");
        }
        return Result::Err("w0 test runtime");
    }

    if (!expected_lines.is_empty()) {
        if (!expected_lines.all(|line| combined.contains(line))) {
            if (verbose) {
                std::println("  output mismatch:");
                std::println($"  actual: {combined}");
                foreach (const line in expected_lines) {
                    std::println($"  expected: {line}");
                }
            }
            return Result::Err("output");
        }
    }

    // Check RC leaks
    if (!check_rc_leaks(error_output)) {
        if (verbose) {
            std::println("  rc leak detected:");
            var leaked = get_leaked_addresses(error_output);
            foreach (const addr in leaked) {
                std::println($"  leaked: {addr}");
            }
        }
        return Result::Err("rc leak");
    }

    return Result::Ok(true);
}

func run_error_test(file: string, w0: string, lib_path: string, verbose: bool) -> Result<string, string> {
    var cmd = $"{w0} --lib-path {lib_path} --check {file}";
    var {output, error_output, exit_code} = std::exec(cmd);

    // Should fail (non-zero exit)
    if (exit_code == 0) {
        if (verbose) {
            std::println("  should have failed but succeeded");
        }
        return Result::Err("should have failed");
    }

    var o = $"{output}{error_output}";

    // Must contain "Error:"
    if (!o.contains("Error:")) {
        if (verbose) {
            std::println("  no error message in output:");
            std::println($"  {o}");
        }
        return Result::Err("no error message");
    }

    // Check expected error text
    var expected = read_expected_lines(file, "Expected error");
    if (!expected.is_empty()) {
        if (!expected.all(|line| o.contains(line))) {
            if (verbose) {
                std::println("  wrong error message:");
                std::println($"  output: {o}");
                foreach (const line in expected) {
                    std::println($"  expected: {line}");
                }
            }
            return Result::Err("wrong error");
        }
    }

    // Return actual error message for display
    var actual = extract_error_message(o);
    return Result::Ok(actual);
}

// --- Main ---

func main() -> i32 {
    const args = std::args();

    var run_valid = args.any(|x| x == "--run" || x == "--valid");
    var run_errors = args.any(|x| x == "--errors");
    var verbose = args.any(|x| x == "--verbose");

    // Collect non-flag arguments as test name filter
    var filter: Option<string>;
    foreach (const arg in args) {
        if (!arg.starts_with("--") && !arg.starts_with("-") && !arg.ends_with("test_runner")) {
            filter = Option::Some(arg);
        }
    }

    if (args.any(|x| x == "--help")) {
        std::println("""
        Usage: test_runner [OPTIONS] [FILTER]

        Options:
          --run       Run only executable program tests (test/run/**)
          --errors    Run only error case tests (test/errors/**)
          --verbose   Show per-test pass/fail and detailed output on failure
          --help      Show this help

        Filter:
          Optional substring to match against test file paths.
          Example: test_runner option_struct_field

        With no options, runs all tests.
        """);
        return 0;
    }

    // Default: run all
    if (!run_valid && !run_errors) {
        run_valid = true;
        run_errors = true;
    }

    var w0 = "bin/w0";
    var lib_path = "../lib";

    // Check that w0 binary exists
    if (!fs::file_exists(w0)) {
        std::println($"{red($"Error: {w0} not found. Run 'make' first.")}");
        return 1;
    }

    var run_passed: i64 = 0;
    var run_failed: i64 = 0;
    var run_skipped: i64 = 0;
    var error_passed: i64 = 0;
    var error_failed: i64 = 0;

    std::println($"{blue("=== Running W0 Test Suite ===")}");
    std::println("");

    if (run_valid) {
        std::println($"{cyan("=== Run Tests (test/run/**) ===")}");

        var files = fs::walk_dir("test/run").filter(|f| f.ends_with(".w"));
        files.sort();

        foreach (const file in files) {
            if (filter.has_value() && !file.contains(filter.value())) {
                continue;
            }

            var disp = display_path(file);

            var has_test_blocks = file_contains(file, "test \"");
            var has_main = file_contains(file, "func main(");

            if (has_test_blocks) {
                var result = run_test_block_file(file, w0, lib_path, verbose);
                if (result.is_ok()) {
                    run_passed += 1;
                    if (verbose) {
                        std::println($"{disp}:".pad_right(45, ' ') + $" {green("PASS")}");
                    }
                } else {
                    std::println($"{disp}:".pad_right(45, ' ') + $" {red("FAIL")} ({result.error()})");
                    run_failed += 1;
                }
            } else if (has_main) {
                var result = run_program_test(file, w0, lib_path, verbose);
                if (result.is_ok()) {
                    run_passed += 1;
                    if (verbose) {
                        std::println($"{disp}:".pad_right(45, ' ') + $" {green("PASS")}");
                    }
                } else {
                    std::println($"{disp}:".pad_right(45, ' ') + $" {red("FAIL")} ({result.error()})");
                    run_failed += 1;
                }
            } else {
                run_skipped += 1;
            }
        }
        std::println("");
    }

    if (run_errors) {
        std::println($"{cyan("=== Error Cases (test/errors/**) ===")}");

        var files = fs::walk_dir("test/errors").filter(|f| f.ends_with(".w"));
        files.sort();

        foreach (const file in files) {
            if (filter.has_value() && !file.contains(filter.value())) {
                continue;
            }

            var disp = display_path(file);

            var result = run_error_test(file, w0, lib_path, verbose);
            if (result.is_ok()) {
                error_passed += 1;
                if (verbose) {
                    std::println($"{disp}:".pad_right(45, ' ') + $" {green("PASS")}");
                }
            } else {
                std::println($"{disp}:".pad_right(45, ' ') + $" {red("FAIL")} ({result.error()})");
                error_failed += 1;
            }
        }
        std::println("");
    }

    // Summary
    std::println($"{blue("=== Test Summary ===")}");

    if (run_valid) {
        var run_total = run_passed + run_failed;
        std::println($"{green($"Run Tests:      {run_passed}/{run_total} passed")}");
        if (run_skipped > 0) {
            std::println($"{gray($"Run Skipped:    {run_skipped} helper module(s)")}");
        }
    }

    if (run_errors) {
        var error_total = error_passed + error_failed;
        std::println($"{green($"Error Cases:    {error_passed}/{error_total} passed")}");
    }

    var total_passed = run_passed + error_passed;
    var total_failed = run_failed + error_failed;
    var total = total_passed + total_failed;

    if (run_valid && run_errors) {
        std::println($"{cyan($"Total:          {total_passed}/{total} tests passed")}");
    }

    if (total_failed == 0) {
        std::println($"{green("All tests passed!")}");
        return 0;
    } else {
        std::println($"{red($"{total_failed} test(s) failed")}");
        return 1;
    }
}
