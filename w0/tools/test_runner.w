// Whist Test Runner
// Replaces scripts/run_tests.sh — dogfooding the compiler.

import std;
import fs;
import collections;

// --- Helpers ---

func collect_files(dir: string, files: Vec<string>): void {
    var dh = fs.open_dir(dir);
    if (dh == null) {
        return;
    }

    var dirs = new Vec<string>{};
    var entry = fs.read_dir(dh);
    while (entry != "") {
        var path = fs.join_path(dir, entry);
        if (fs.is_dir(path)) {
            dirs.push(path);
        } else if (entry.ends_with(".w")) {
            files.push(path);
        }
        entry = fs.read_dir(dh);
    }
    fs.close_dir(dh);

    // Recurse into subdirectories
    var i: i64 = 0;
    while (i < dirs.count) {
        collect_files(dirs[i], files);
        i = i + 1;
    }
}


func read_expected_lines(path: string, prefix: string): Vec<string> {
    var lines = new Vec<string>{};
    var marker = "// " + prefix + ": ";
    foreach (const line in fs.read_file(path).split("\n")) {
        if (line.starts_with(marker)) {
            lines.push(line[marker.length():line.length()]);
        }
    }
    return lines;
}

func file_contains(path: string, pattern: string): bool {
    var content = fs.read_file(path);
    return content.contains(pattern);
}

func display_path(file: string): string {
    // Strip "test/" prefix for display
    if (file.starts_with("test/")) {
        return file[5:file.length()];
    }
    return file;
}

func check_output_contains(output: string, expected: Vec<string>): bool {
    var i: i64 = 0;
    while (i < expected.count) {
        if (!output.contains(expected[i])) {
            return false;
        }
        i = i + 1;
    }
    return true;
}

// --- Test runners ---

func run_test_block_file(file: string, w0: string, lib_path: string, verbose: bool): bool {
    var cmd = w0 + " --lib-path " + lib_path + " test " + file;
    var result = std.exec(cmd);

    var combined = result.output + result.error_output;
    var expected = read_expected_lines(file, "Expected");

    if (expected.count > 0) {
        if (!check_output_contains(combined, expected)) {
            if (verbose) {
                std.println("  command: " + cmd);
                std.println("  output:  " + combined);
            }
            return false;
        }
        return true;
    }

    // No expected lines — just check exit code
    if (result.exit_code != 0) {
        if (verbose) {
            std.println("  command: " + cmd);
            std.println("  output:  " + combined);
        }
        return false;
    }
    return true;
}

func run_program_test(file: string, w0: string, lib_path: string, verbose: bool): bool {
    // Compile
    var compile_cmd = w0 + " --lib-path " + lib_path + " " + file + " | cc -x c -I" + lib_path + "/include -o /tmp/whist_test_bin - " + lib_path + "/whist_runtime.c";
    var compile_result = std.exec(compile_cmd);
    if (compile_result.exit_code != 0) {
        if (verbose) {
            std.println("  compile failed:");
            std.println("  " + compile_result.error_output);
        }
        return false;
    }

    // Run
    var run_result = std.exec("/tmp/whist_test_bin");

    // Check expected exit code
    var expected_exit_lines = read_expected_lines(file, "Expected exit");
    var expected_exit: i64 = 0;
    if (expected_exit_lines.count > 0) {
        expected_exit = std.parse_i64(expected_exit_lines[0]);
    }

    var actual_exit: i64 = run_result.exit_code as i64;
    if (actual_exit != expected_exit) {
        if (verbose) {
            std.println($"  exit code: {actual_exit} (expected {expected_exit})");
            std.println("  stdout: " + run_result.output);
            std.println("  stderr: " + run_result.error_output);
        }
        return false;
    }

    // Check expected stdout
    var expected_stdout = read_expected_lines(file, "Expected");
    if (expected_stdout.count > 0) {
        if (!check_output_contains(run_result.output, expected_stdout)) {
            if (verbose) {
                std.println("  stdout mismatch:");
                std.println("  actual: " + run_result.output);
                var k: i64 = 0;
                while (k < expected_stdout.count) {
                    std.println("  expected line: " + expected_stdout[k]);
                    k = k + 1;
                }
            }
            return false;
        }
    }

    // Check expected stderr
    var expected_stderr = read_expected_lines(file, "Expected stderr");
    if (expected_stderr.count > 0) {
        if (!check_output_contains(run_result.error_output, expected_stderr)) {
            if (verbose) {
                std.println("  stderr mismatch:");
                std.println("  actual: " + run_result.error_output);
            }
            return false;
        }
    }

    return true;
}

func run_error_test(file: string, w0: string, lib_path: string, verbose: bool): bool {
    var cmd = w0 + " --lib-path " + lib_path + " --check " + file;
    var result = std.exec(cmd);

    // Should fail (non-zero exit)
    if (result.exit_code == 0) {
        if (verbose) {
            std.println("  should have failed but succeeded");
        }
        return false;
    }

    var output = result.output + result.error_output;

    // Must contain "Error:"
    if (!output.contains("Error:")) {
        if (verbose) {
            std.println("  no error message in output:");
            std.println("  " + output);
        }
        return false;
    }

    // Check expected error text
    var expected = read_expected_lines(file, "Expected error");
    if (expected.count > 0) {
        if (!check_output_contains(output, expected)) {
            if (verbose) {
                std.println("  wrong error message:");
                std.println("  output: " + output);
                var k: i64 = 0;
                while (k < expected.count) {
                    std.println("  expected: " + expected[k]);
                    k = k + 1;
                }
            }
            return false;
        }
    }

    return true;
}

// --- Main ---

func main(): i32 {
    var args = std.args();

    var run_valid = false;
    var run_errors = false;
    var verbose = false;

    var i: i64 = 1;
    while (i < args.count) {
        if (args[i] == "--run" || args[i] == "--valid") {
            run_valid = true;
        } else if (args[i] == "--errors") {
            run_errors = true;
        } else if (args[i] == "--verbose") {
            verbose = true;
        } else if (args[i] == "--help") {
            std.println("Usage: test_runner [OPTIONS]");
            std.println("");
            std.println("Options:");
            std.println("  --run       Run only executable program tests (test/run/**)");
            std.println("  --errors    Run only error case tests (test/errors/**)");
            std.println("  --verbose   Show detailed output on failure");
            std.println("  --help      Show this help");
            std.println("");
            std.println("With no options, runs all tests.");
            return 0;
        }
        i = i + 1;
    }

    // Default: run all
    if (!run_valid && !run_errors) {
        run_valid = true;
        run_errors = true;
    }

    var w0 = "bin/w0";
    var lib_path = "../lib";

    var run_passed: i64 = 0;
    var run_failed: i64 = 0;
    var run_skipped: i64 = 0;
    var error_passed: i64 = 0;
    var error_failed: i64 = 0;

    std.println("=== Running W0 Test Suite ===");
    std.println("");

    if (run_valid) {
        std.println("=== Run Tests (test/run/**) ===");

        var files = new Vec<string>{};
        collect_files("test/run", files);
        files.sort();

        var j: i64 = 0;
        while (j < files.count) {
            var file = files[j];
            var disp = display_path(file);

            var has_test_blocks = file_contains(file, "test \"");
            var has_main = file_contains(file, "func main(");

            if (has_test_blocks) {
                std.print($"{disp}: ");
                var ok = run_test_block_file(file, w0, lib_path, verbose);
                if (ok) {
                    std.println("PASS");
                    run_passed = run_passed + 1;
                } else {
                    std.println("FAIL");
                    run_failed = run_failed + 1;
                }
            } else if (has_main) {
                std.print($"{disp}: ");
                var ok = run_program_test(file, w0, lib_path, verbose);
                if (ok) {
                    std.println("PASS");
                    run_passed = run_passed + 1;
                } else {
                    std.println("FAIL");
                    run_failed = run_failed + 1;
                }
            } else {
                if (verbose) {
                    std.println($"{disp}: SKIP (helper module)");
                }
                run_skipped = run_skipped + 1;
            }

            j = j + 1;
        }
        std.println("");
    }

    if (run_errors) {
        std.println("=== Error Cases (test/errors/**) ===");

        var files = new Vec<string>{};
        collect_files("test/errors", files);
        files.sort();

        var j: i64 = 0;
        while (j < files.count) {
            var file = files[j];
            var disp = display_path(file);

            std.print($"{disp}: ");
            var ok = run_error_test(file, w0, lib_path, verbose);
            if (ok) {
                std.println("PASS (correct error)");
                error_passed = error_passed + 1;
            } else {
                std.println("FAIL");
                error_failed = error_failed + 1;
            }

            j = j + 1;
        }
        std.println("");
    }

    // Summary
    std.println("=== Test Summary ===");

    if (run_valid) {
        var run_total = run_passed + run_failed;
        std.println($"Run Tests:      {run_passed}/{run_total} passed");
        if (run_skipped > 0) {
            std.println($"Run Skipped:    {run_skipped} helper module(s)");
        }
    }

    if (run_errors) {
        var error_total = error_passed + error_failed;
        std.println($"Error Cases:    {error_passed}/{error_total} passed");
    }

    var total_passed = run_passed + error_passed;
    var total_failed = run_failed + error_failed;
    var total = total_passed + total_failed;

    if (run_valid && run_errors) {
        std.println($"Total:          {total_passed}/{total} tests passed");
    }

    if (total_failed == 0) {
        std.println("All tests passed!");
        return 0;
    } else {
        std.println($"{total_failed} test(s) failed");
        return 1;
    }
}
