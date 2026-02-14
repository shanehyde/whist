// Expected: PASS: import_fs
// Test importing the fs library with module qualification

import fs;

test "import_fs" {
    // Convenience API
    var result = fs.write_file("/tmp/whist_fs_test.txt", "hello whist");
    var exists = fs.file_exists("/tmp/whist_fs_test.txt");
    var content = fs.read_file("/tmp/whist_fs_test.txt");
    var size = fs.file_size("/tmp/whist_fs_test.txt");
    var append_result = fs.append_file("/tmp/whist_fs_test.txt", " appended");
    var rename_result = fs.rename_file("/tmp/whist_fs_test.txt", "/tmp/whist_fs_test2.txt");
    var remove_result = fs.remove_file("/tmp/whist_fs_test2.txt");

    // Handle-based API
    var h = fs.open("/tmp/whist_fs_handle_test.txt", "w");
    var write_result = fs.write_string(h, "line one\nline two\n");
    var flush_result = fs.flush(h);
    var close_result = fs.close(h);

    var h2 = fs.open("/tmp/whist_fs_handle_test.txt", "r");
    var line = fs.read_line(h2);
    var pos = fs.tell(h2);
    var seek_result = fs.seek(h2, 0, 0);
    var at_eof = fs.eof(h2);
    fs.close(h2);

    fs.remove_file("/tmp/whist_fs_handle_test.txt");
}
