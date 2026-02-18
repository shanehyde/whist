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

func (Timer) timelapsed(): i64 {
    return time::time_ms() - self.start;
}

impl Drop for Timer {
    func drop() {
        var elapsed = self.timelapsed();
        std::println($"{self.desc} took {elapsed} ms");
    }
}

// --- ANSI colors ---

func ansi(code: string): string {
    var sb = new StringBuilder{};
    sb.append_char(27 as char);
    sb.append("[");
    sb.append(code);
    sb.append("m");
    return sb.to_string();
}

// --- RC helpers ---

func extract_rc_addresses(output: string, prefix: string): Vec<string> {
    var addrs = new Vec<string>{};
    foreach (const line in output.split("\n")) {
        if (line.starts_with(prefix)) {
            var parts = line.split(" ");
            if (parts.count > 1) {
                addrs.push(parts[1]);
            }
        }
    }
    addrs.sort();
    return addrs;
}

func build_address_set(addrs: Vec<string>): Set<string> {
    var s = new Set<string>{
        buckets: new Vec<SetEntry<string>>{},
        count: 0, capacity: 0,
    };
    var cap: i64 = 64;
    if (addrs.count > cap) {
        cap = addrs.count * 2;
    }
    s.init(cap);
    foreach (const addr in addrs) {
        s.insert(addr);
    }
    return s;
}

func check_rc_leaks(stderr: string): bool {
    var allocs = extract_rc_addresses(stderr, "RC_ALLOC:");
    var frees = extract_rc_addresses(stderr, "RC_FREE:");
    if (allocs.count == 0) {
        return true;
    }
    var free_set = build_address_set(frees);
    foreach (const addr in allocs) {
        if (!free_set.contains(addr)) {
            return false;
        }
    }
    return true;
}

func get_leaked_addresses(stderr: string): Vec<string> {
    var allocs = extract_rc_addresses(stderr, "RC_ALLOC:");
    var frees = extract_rc_addresses(stderr, "RC_FREE:");
    var free_set = build_address_set(frees);
    var leaked = new Vec<string>{};
    foreach (const addr in allocs) {
        if (!free_set.contains(addr)) {
            leaked.push(addr);
        }
    }
    return leaked;
}

func check_rc_free_order(stderr: string, expected: string): bool {
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

func filter_rc_lines(output: string): string {
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

func collect_files(dir: string, files: Vec<string>): void {
    var dh = fs::open_dir(dir);
    if (dh == null) {
        return;
    }

    var dirs = new Vec<string>{};
    var entry = fs::read_dir(dh);
    while (entry != "") {
        var path = fs::join_path(dir, entry);
        if (fs::is_dir(path)) {
            dirs.push(path);
        } else if (entry.ends_with(".w")) {
            files.push(path);
        }
        entry = fs::read_dir(dh);
    }
    fs::close_dir(dh);

    foreach(const dir in dirs) {
        collect_files(dir, files);
    }
}

func read_expected_lines(path: string, prefix: string): Vec<string> {
    var lines = new Vec<string>{};
    var marker = "// " + prefix + ": ";

    foreach (const line in fs::read_file(path).split("\n")) {
        if (line.starts_with(marker)) {
            lines.push(line.strip_prefix(marker));
        }
    }
    return lines;
}

func file_contains(path: string, pattern: string): bool {
    var content = fs::read_file(path);
    return content.contains(pattern);
}

func display_path(file: string): string {
    return file.strip_prefix("test/");
}

func check_output_contains(output: string, expected: Vec<string>): bool {
    foreach (const line in expected) {
        if (!output.contains(line)) {
            return false;
        }
    }
    return true;
}

func extract_error_message(output: string): string {
    foreach (const line in output.split("\n")) {
        var idx = line.index_of("Error:");
        if (idx >= 0) {
            return line[idx + 6 : line.length()].trim_start();
        }
    }
    return "";
}

// --- Test runners ---

func run_program_test(file: string, w0: string, lib_path: string, verbose: bool): Result<bool, string> {
    var tmp_bin = "/tmp/whist_test_bin";

    // Step 1: Compile with --rc-debug
    var compile_cmd = $"{w0} --rc-debug --lib-path {lib_path} {file} | cc -x c -I{lib_path}/include -o {tmp_bin} - {lib_path}/whist_runtime.c";
    var compile_result = std::exec(compile_cmd);
    if (compile_result.exit_code != 0) {
        if (verbose) {
            std::println("  command: " + compile_cmd);
            std::println("  output:  " + compile_result.error_output);
        }
        return Result::Err("compile");
    }

    // Step 2: Run the compiled binary
    var run_result = std::exec(tmp_bin);
    fs::remove_file(tmp_bin);

    // Check expected exit code
    var expected_exit_lines = read_expected_lines(file, "Expected exit");
    var expected_exit: i64 = 0;
    if (expected_exit_lines.count > 0) {
        expected_exit = std::parse_i64(expected_exit_lines[0]);
    }

    var actual_exit: i64 = run_result.exit_code as i64;
    if (actual_exit != expected_exit) {
        if (verbose) {
            std::println($"  exit code: {actual_exit} (expected {expected_exit})");
            std::println("  stdout: " + run_result.output);
            std::println("  stderr: " + run_result.error_output);
        }
        return Result::Err($"exit {actual_exit}, expected {expected_exit}");
    }

    // Check expected stdout
    var expected_stdout = read_expected_lines(file, "Expected");
    if (expected_stdout.count > 0) {
        if (!check_output_contains(run_result.output, expected_stdout)) {
            if (verbose) {
                std::println("  stdout mismatch:");
                std::println("  actual: " + run_result.output);
                foreach (const line in expected_stdout) {
                    std::println("  expected line: " + line);
                }
            }
            return Result::Err("stdout");
        }
    }

    // Check expected stderr (filter RC debug lines first)
    var expected_stderr = read_expected_lines(file, "Expected stderr");
    if (expected_stderr.count > 0) {
        var filtered_stderr = filter_rc_lines(run_result.error_output);
        if (!check_output_contains(filtered_stderr, expected_stderr)) {
            if (verbose) {
                std::println("  stderr mismatch:");
                std::println("  actual: " + filtered_stderr);
            }
            return Result::Err("stderr");
        }
    }

    // Check expected RC free order
    var expected_order_lines = read_expected_lines(file, "Expected rc free order");
    if (expected_order_lines.count > 0) {
        if (!check_rc_free_order(run_result.error_output, expected_order_lines[0])) {
            if (verbose) {
                std::println("  rc free order mismatch:");
                std::println("  expected: " + expected_order_lines[0]);
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
                    std::println("  leaked: " + addr);
                }
            }
            return Result::Err("rc leak");
        }
    }

    return Result::Ok(true);
}

func run_test_block_file(file: string, w0: string, lib_path: string, verbose: bool): Result<bool, string> {
    var cmd = $"{w0} --rc-debug --lib-path {lib_path} test {file}";
    var {output, error_output, exit_code} = std::exec(cmd);

    // Build combined output with RC lines filtered from stderr
    var combined = output + filter_rc_lines(error_output);

    // Check expected output lines
    var expected_lines = read_expected_lines(file, "Expected");

    if (exit_code != 0 && expected_lines.count == 0) {
        if (verbose) {
            std::println("  command: " + cmd);
            std::println("  output:  " + combined);
        }
        return Result::Err("w0 test runtime");
    }

    if (expected_lines.count > 0) {
        if (!check_output_contains(combined, expected_lines)) {
            if (verbose) {
                std::println("  output mismatch:");
                std::println("  actual: " + combined);
                foreach (const line in expected_lines) {
                    std::println("  expected: " + line);
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
                std::println("  leaked: " + addr);
            }
        }
        return Result::Err("rc leak");
    }

    return Result::Ok(true);
}

func run_error_test(file: string, w0: string, lib_path: string, verbose: bool): Result<string, string> {
    var cmd = $"{w0} --lib-path {lib_path} --check {file}";
    var {output, error_output, exit_code} = std::exec(cmd);

    // Should fail (non-zero exit)
    if (exit_code == 0) {
        if (verbose) {
            std::println("  should have failed but succeeded");
        }
        return Result::Err("should have failed");
    }

    var o = output + error_output;

    // Must contain "Error:"
    if (!o.contains("Error:")) {
        if (verbose) {
            std::println("  no error message in output:");
            std::println("  " + o);
        }
        return Result::Err("no error message");
    }

    // Check expected error text
    var expected = read_expected_lines(file, "Expected error");
    if (expected.count > 0) {
        if (!check_output_contains(o, expected)) {
            if (verbose) {
                std::println("  wrong error message:");
                std::println("  output: " + o);
                foreach (const line in expected) {
                    std::println("  expected: " + line);
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

func main(): i32 {
    const args = std::args();

    var run_valid = args.any(|x| x == "--run" || x == "--valid");
    var run_errors = args.any(|x| x == "--errors");
    var verbose = args.any(|x| x == "--verbose");

    if (args.any(|x| x == "--help")) {
        std::println("""
        Usage: test_runner [OPTIONS]

        Options:
          --run       Run only executable program tests (test/run/**)
          --errors    Run only error case tests (test/errors/**)
          --verbose   Show detailed output on failure
          --help      Show this help

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
        std::println(ansi("1;31") + $"Error: {w0} not found. Run 'make' first." + ansi("0"));
        return 1;
    }

    var run_passed: i64 = 0;
    var run_failed: i64 = 0;
    var run_skipped: i64 = 0;
    var error_passed: i64 = 0;
    var error_failed: i64 = 0;

    std::println(ansi("1;34") + "=== Running W0 Test Suite ===" + ansi("0"));
    std::println("");

    if (run_valid) {
        std::println(ansi("1;36") + "=== Run Tests (test/run/**) ===" + ansi("0"));

        var files = new Vec<string>{};
        collect_files("test/run", files);
        files.sort();

        foreach (const file in files) {
            var disp = display_path(file);

            var has_test_blocks = file_contains(file, "test \"");
            var has_main = file_contains(file, "func main(");

            if (has_test_blocks) {
                std::print($"{disp}:".pad_right(45, ' '));
                var result = run_test_block_file(file, w0, lib_path, verbose);
                if (result.is_ok()) {
                    std::println(" " + ansi("1;32") + "PASS" + ansi("0"));
                    run_passed += 1;
                } else {
                    std::println(" " + ansi("1;31") + "FAIL (" + result.error() + ")" + ansi("0"));
                    run_failed += 1;
                }
            } else if (has_main) {
                std::print($"{disp}:".pad_right(45, ' '));
                var result = run_program_test(file, w0, lib_path, verbose);
                if (result.is_ok()) {
                    std::println(" " + ansi("1;32") + "PASS" + ansi("0"));
                    run_passed += 1;
                } else {
                    std::println(" " + ansi("1;31") + "FAIL (" + result.error() + ")" + ansi("0"));
                    run_failed += 1;
                }
            } else {
                if (verbose) {
                    std::println(ansi("90") + $"{disp}: SKIP (helper module)" + ansi("0"));
                }
                run_skipped += 1;
            }
        }
        std::println("");
    }

    if (run_errors) {
        std::println(ansi("1;36") + "=== Error Cases (test/errors/**) ===" + ansi("0"));

        var files = new Vec<string>{};
        collect_files("test/errors", files);
        files.sort();

        foreach (const file in files) {
            var disp = display_path(file);

            std::print($"{disp}:".pad_right(45, ' '));

            var result = run_error_test(file, w0, lib_path, verbose);
            if let Ok(actual) = result {
                std::println(" " + ansi("1;32") + "PASS (correct error)" + ansi("0"));
                if (actual != "") {
                    std::println("  " + ansi("90") + actual + ansi("0"));
                }
                error_passed += 1;
            } else {
                std::println(" " + ansi("1;31") + "FAIL (" + result.error() + ")" + ansi("0"));
                error_failed += 1;
            }
        }
        std::println("");
    }

    // Summary
    std::println(ansi("1;34") + "=== Test Summary ===" + ansi("0"));

    if (run_valid) {
        var run_total = run_passed + run_failed;
        std::println(ansi("1;32") + $"Run Tests:      {run_passed}/{run_total} passed" + ansi("0"));
        if (run_skipped > 0) {
            std::println(ansi("90") + $"Run Skipped:    {run_skipped} helper module(s)" + ansi("0"));
        }
    }

    if (run_errors) {
        var error_total = error_passed + error_failed;
        std::println(ansi("1;32") + $"Error Cases:    {error_passed}/{error_total} passed" + ansi("0"));
    }

    var total_passed = run_passed + error_passed;
    var total_failed = run_failed + error_failed;
    var total = total_passed + total_failed;

    if (run_valid && run_errors) {
        std::println(ansi("1;36") + $"Total:          {total_passed}/{total} tests passed" + ansi("0"));
    }

    if (total_failed == 0) {
        std::println(ansi("1;32") + "All tests passed!" + ansi("0"));
        return 0;
    } else {
        std::println(ansi("1;31") + $"{total_failed} test(s) failed" + ansi("0"));
        return 1;
    }
}
