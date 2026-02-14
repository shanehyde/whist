// Expected: PASS: fs_operations
// Test fs module: validates return values and verifies file operations

import fs;

func test_write_and_read(): void {
    var rc = fs.write_file("/tmp/whist_fs_ops_test.txt", "hello whist");
    assert(rc == 0);

    // Verify content was written by checking size
    var size = fs.file_size("/tmp/whist_fs_ops_test.txt");
    assert(size == 11);

    // Read it back to exercise the call
    fs.read_file("/tmp/whist_fs_ops_test.txt");
}

func test_append(): void {
    fs.write_file("/tmp/whist_fs_ops_append.txt", "first");
    var rc = fs.append_file("/tmp/whist_fs_ops_append.txt", " second");
    assert(rc == 0);

    var size = fs.file_size("/tmp/whist_fs_ops_append.txt");
    assert(size == 12);

    fs.remove_file("/tmp/whist_fs_ops_append.txt");
}

func test_file_exists(): void {
    fs.write_file("/tmp/whist_fs_ops_exists.txt", "x");
    assert(fs.file_exists("/tmp/whist_fs_ops_exists.txt") == true);

    fs.remove_file("/tmp/whist_fs_ops_exists.txt");
    assert(fs.file_exists("/tmp/whist_fs_ops_exists.txt") == false);
}

func test_rename(): void {
    fs.write_file("/tmp/whist_fs_ops_rename_a.txt", "data");
    var rc = fs.rename_file("/tmp/whist_fs_ops_rename_a.txt", "/tmp/whist_fs_ops_rename_b.txt");
    assert(rc == 0);
    assert(fs.file_exists("/tmp/whist_fs_ops_rename_a.txt") == false);
    assert(fs.file_exists("/tmp/whist_fs_ops_rename_b.txt") == true);

    fs.remove_file("/tmp/whist_fs_ops_rename_b.txt");
}

func test_remove(): void {
    fs.write_file("/tmp/whist_fs_ops_remove.txt", "gone");
    var rc = fs.remove_file("/tmp/whist_fs_ops_remove.txt");
    assert(rc == 0);
    assert(fs.file_exists("/tmp/whist_fs_ops_remove.txt") == false);
}

func test_handle_write_read(): void {
    // Write via handle
    var wh = fs.open("/tmp/whist_fs_ops_handle.txt", "w");
    assert(wh != null);

    var rc = fs.write_string(wh, "line one\n");
    assert(rc == 0);
    fs.write_string(wh, "line two\n");
    fs.flush(wh);
    fs.close(wh);

    // Read back via handle
    var rh = fs.open("/tmp/whist_fs_ops_handle.txt", "r");
    assert(rh != null);

    assert(fs.eof(rh) == false);

    // Read two lines, verifying via tell that position advances
    fs.read_line(rh);
    var pos_after_line1 = fs.tell(rh);
    assert(pos_after_line1 == 9);

    fs.read_line(rh);
    var pos_after_line2 = fs.tell(rh);
    assert(pos_after_line2 == 18);

    fs.close(rh);
    fs.remove_file("/tmp/whist_fs_ops_handle.txt");
}

func test_seek_tell(): void {
    fs.write_file("/tmp/whist_fs_ops_seek.txt", "abcdefghij");

    var h = fs.open("/tmp/whist_fs_ops_seek.txt", "r");
    assert(h != null);

    var pos0 = fs.tell(h);
    assert(pos0 == 0);

    // Read 5 bytes to advance position
    fs.read_line(h);
    var pos1 = fs.tell(h);
    assert(pos1 == 10);

    // Seek back to start
    var rc = fs.seek(h, 0, 0);
    assert(rc == 0);
    var pos2 = fs.tell(h);
    assert(pos2 == 0);

    // Seek to offset 5 from start
    fs.seek(h, 5, 0);
    var pos3 = fs.tell(h);
    assert(pos3 == 5);

    // Seek relative +2 from current
    fs.seek(h, 2, 1);
    var pos4 = fs.tell(h);
    assert(pos4 == 7);

    // Seek from end
    fs.seek(h, -3, 2);
    var pos5 = fs.tell(h);
    assert(pos5 == 7);

    fs.close(h);
    fs.remove_file("/tmp/whist_fs_ops_seek.txt");
}

func test_error_cases(): void {
    assert(fs.file_exists("/tmp/whist_fs_ops_nonexistent.txt") == false);

    var size = fs.file_size("/tmp/whist_fs_ops_nonexistent.txt");
    assert(size == -1);

    // Opening non-existent for read should return 0 (null handle)
    var h = fs.open("/tmp/whist_fs_ops_nonexistent.txt", "r");
    assert(h == null);

    // Remove on non-existent should fail
    var rc = fs.remove_file("/tmp/whist_fs_ops_nonexistent.txt");
    assert(rc == -1);
}

test "fs_operations" {
    test_write_and_read();
    test_append();
    test_file_exists();
    test_rename();
    test_remove();
    test_handle_write_read();
    test_seek_tell();
    test_error_cases();

    // Cleanup the file from test_write_and_read
    fs.remove_file("/tmp/whist_fs_ops_test.txt");
}
