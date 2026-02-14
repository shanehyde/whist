// Expected: PASS: exec captures exit code
// Expected: PASS: exec captures output
// Expected: PASS: exec captures nonzero exit
// Expected: PASS: exec captures stderr
import std;

test "exec captures exit code" {
    var result = std.exec("echo hello");
    assert(result.exit_code == 0);
}

test "exec captures output" {
    var result = std.exec("echo hello");
    assert(result.output == "hello\n");
}

test "exec captures nonzero exit" {
    var result = std.exec("exit 42");
    assert(result.exit_code == 42);
}

test "exec captures stderr" {
    var result = std.exec("echo oops >&2");
    assert(result.error_output == "oops\n");
}
