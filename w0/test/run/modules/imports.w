// Expected: PASS: import_fs
// Expected: PASS: import_std
// Expected: PASS: import_relative
// Expected: PASS: import_parent
// Expected: PASS: relative_import_chain

import std;
import fs;
include "./util/inner/helper.w";
include "./util/combined_helper.w";

test "import_fs" {
    // Convenience API
    var result = fs::write_file("/tmp/whist_fs_test.txt", "hello whist");
    var exists = fs::file_exists("/tmp/whist_fs_test.txt");
    var content = fs::read_file("/tmp/whist_fs_test.txt");
    var size = fs::file_size("/tmp/whist_fs_test.txt");
    var append_result = fs::append_file("/tmp/whist_fs_test.txt", " appended");
    var rename_result = fs::rename_file("/tmp/whist_fs_test.txt", "/tmp/whist_fs_test2.txt");
    var remove_result = fs::remove_file("/tmp/whist_fs_test2.txt");

    // Handle-based API
    var h = fs::open("/tmp/whist_fs_handle_test.txt", "w");
    var write_result = fs::write_string(h, "line one\nline two\n");
    var flush_result = fs::flush(h);
    var close_result = fs::close(h);

    var h2 = fs::open("/tmp/whist_fs_handle_test.txt", "r");
    var line = fs::read_line(h2);
    var pos = fs::tell(h2);
    var seek_result = fs::seek(h2, 0, 0);
    var at_eof = fs::eof(h2);
    fs::close(h2);

    fs::remove_file("/tmp/whist_fs_handle_test.txt");
}

test "import_std" {
    var a = std::abs_i64(-42);
    var b = std::max_i64(10, 20);
    var c = std::min_i64(5, 3);
    assert(a == 42);
    assert(b == 20);
    assert(c == 3);
}

test "import_relative" {
    var a = twice(5);
    var b = triple(4);
    assert(a == 10);
    assert(b == 12);
}

test "import_parent" {
    var a = quadruple(3);
    assert(a == 12);
}

test "relative_import_chain" {
    // This calls a function from combined_helper.w
    var a = apply_both(5);  // (5 + 2) * 3 = 21

    // These call functions from math_helper.w (merged through combined_helper)
    var b = add_two(10);        // 12
    var c = multiply_three(4);  // 12

    assert(a == 21);
    assert(b == 12);
    assert(c == 12);
}
