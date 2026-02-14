// Test fs module: validates return values and verifies file operations
// Expected: PASS: fs_operations.w
//
// Type-checks via `make test`. To run the actual operations:
//   bin/w0 --lib-path ../lib test/run/stdlib/fs_operations.w | cc -x c -I../lib/include -o /tmp/fs_ops -
//   /tmp/fs_ops

import fs;
import std;

func check(cond: bool, msg: string): void {
    if (!cond) {
        std.print("FAIL: ");
        std.print(msg);
        std.print("\n");
    }
}

func test_write_and_read(): void {
    var rc = fs.write_file("/tmp/whist_fs_ops_test.txt", "hello whist");
    check(rc == 0, "write_file should return 0");

    // Verify content was written by checking size
    var size = fs.file_size("/tmp/whist_fs_ops_test.txt");
    check(size == 11, "file_size should be 11 bytes");

    // Read it back to exercise the call
    fs.read_file("/tmp/whist_fs_ops_test.txt");
}

func test_append(): void {
    fs.write_file("/tmp/whist_fs_ops_append.txt", "first");
    var rc = fs.append_file("/tmp/whist_fs_ops_append.txt", " second");
    check(rc == 0, "append_file should return 0");

    var size = fs.file_size("/tmp/whist_fs_ops_append.txt");
    check(size == 12, "appended file should be 12 bytes");

    fs.remove_file("/tmp/whist_fs_ops_append.txt");
}

func test_file_exists(): void {
    fs.write_file("/tmp/whist_fs_ops_exists.txt", "x");
    check(fs.file_exists("/tmp/whist_fs_ops_exists.txt") == true, "file should exist after write");

    fs.remove_file("/tmp/whist_fs_ops_exists.txt");
    check(fs.file_exists("/tmp/whist_fs_ops_exists.txt") == false, "file should not exist after remove");
}

func test_rename(): void {
    fs.write_file("/tmp/whist_fs_ops_rename_a.txt", "data");
    var rc = fs.rename_file("/tmp/whist_fs_ops_rename_a.txt", "/tmp/whist_fs_ops_rename_b.txt");
    check(rc == 0, "rename_file should return 0");
    check(fs.file_exists("/tmp/whist_fs_ops_rename_a.txt") == false, "old name should not exist");
    check(fs.file_exists("/tmp/whist_fs_ops_rename_b.txt") == true, "new name should exist");

    fs.remove_file("/tmp/whist_fs_ops_rename_b.txt");
}

func test_remove(): void {
    fs.write_file("/tmp/whist_fs_ops_remove.txt", "gone");
    var rc = fs.remove_file("/tmp/whist_fs_ops_remove.txt");
    check(rc == 0, "remove_file should return 0");
    check(fs.file_exists("/tmp/whist_fs_ops_remove.txt") == false, "file should be gone");
}

func test_handle_write_read(): void {
    // Write via handle
    var wh = fs.open("/tmp/whist_fs_ops_handle.txt", "w");
    check(wh != null, "open for write should return non-null handle");

    var rc = fs.write_string(wh, "line one\n");
    check(rc == 0, "write_string should return 0");
    fs.write_string(wh, "line two\n");
    fs.flush(wh);
    fs.close(wh);

    // Read back via handle
    var rh = fs.open("/tmp/whist_fs_ops_handle.txt", "r");
    check(rh != null, "open for read should return non-null handle");

    check(fs.eof(rh) == false, "should not be at eof initially");

    // Read two lines, verifying via tell that position advances
    fs.read_line(rh);
    var pos_after_line1 = fs.tell(rh);
    check(pos_after_line1 == 9, "position after first line should be 9");

    fs.read_line(rh);
    var pos_after_line2 = fs.tell(rh);
    check(pos_after_line2 == 18, "position after second line should be 18");

    fs.close(rh);
    fs.remove_file("/tmp/whist_fs_ops_handle.txt");
}

func test_seek_tell(): void {
    fs.write_file("/tmp/whist_fs_ops_seek.txt", "abcdefghij");

    var h = fs.open("/tmp/whist_fs_ops_seek.txt", "r");
    check(h != null, "open for seek test should succeed");

    var pos0 = fs.tell(h);
    check(pos0 == 0, "initial position should be 0");

    // Read 5 bytes to advance position
    fs.read_line(h);
    var pos1 = fs.tell(h);
    check(pos1 == 10, "position after reading 10-byte file should be 10");

    // Seek back to start
    var rc = fs.seek(h, 0, 0);
    check(rc == 0, "seek to start should return 0");
    var pos2 = fs.tell(h);
    check(pos2 == 0, "position after seek to start should be 0");

    // Seek to offset 5 from start
    fs.seek(h, 5, 0);
    var pos3 = fs.tell(h);
    check(pos3 == 5, "position after seek to 5 should be 5");

    // Seek relative +2 from current
    fs.seek(h, 2, 1);
    var pos4 = fs.tell(h);
    check(pos4 == 7, "position after relative seek +2 should be 7");

    // Seek from end
    fs.seek(h, -3, 2);
    var pos5 = fs.tell(h);
    check(pos5 == 7, "position after seek -3 from end should be 7");

    fs.close(h);
    fs.remove_file("/tmp/whist_fs_ops_seek.txt");
}

func test_error_cases(): void {
    check(fs.file_exists("/tmp/whist_fs_ops_nonexistent.txt") == false, "nonexistent file should not exist");

    var size = fs.file_size("/tmp/whist_fs_ops_nonexistent.txt");
    check(size == -1, "file_size on missing file should return -1");

    // Opening non-existent for read should return 0 (null handle)
    var h = fs.open("/tmp/whist_fs_ops_nonexistent.txt", "r");
    check(h == null, "open nonexistent for read should return null");

    // Remove on non-existent should fail
    var rc = fs.remove_file("/tmp/whist_fs_ops_nonexistent.txt");
    check(rc == -1, "remove_file on missing file should return -1");
}

func main(): i32 {
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

    std.print("PASS: fs_operations.w\n");
    return 0;
}
