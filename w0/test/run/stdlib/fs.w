// Expected: PASS: fs_dir_operations
// Expected: PASS: fs_operations

import fs;
import std;

// --- fs_dir_operations ---

const BASE_DIR = "/tmp/whist_fs_dir_test";

func cleanup(): void {
    // Remove any files/dirs from previous runs
    fs::remove_file(fs::join_path(BASE_DIR, "sub/nested/file.txt"));
    fs::rmdir(fs::join_path(BASE_DIR, "sub/nested"));
    fs::rmdir(fs::join_path(BASE_DIR, "sub"));
    fs::remove_file(fs::join_path(BASE_DIR, "hello.txt"));
    fs::remove_file(fs::join_path(BASE_DIR, "a.txt"));
    fs::remove_file(fs::join_path(BASE_DIR, "b.txt"));
    fs::remove_file(fs::join_path(BASE_DIR, "c.txt"));
    fs::rmdir(BASE_DIR);
}

func test_mkdir_rmdir(): void {
    var rc = fs::mkdir(BASE_DIR);
    assert(rc == 0);
    assert(fs::is_dir(BASE_DIR) == true);

    // mkdir on existing dir should fail
    rc = fs::mkdir(BASE_DIR);
    assert(rc == -1);

    // rmdir on empty dir
    rc = fs::rmdir(BASE_DIR);
    assert(rc == 0);
    assert(fs::is_dir(BASE_DIR) == false);

    // rmdir on non-existent should fail
    rc = fs::rmdir(BASE_DIR);
    assert(rc == -1);
}

func test_mkdir_all(): void {
    var nested = fs::join_path(BASE_DIR, "sub/nested");
    var rc = fs::mkdir_all(nested);
    assert(rc == 0);
    assert(fs::is_dir(BASE_DIR) == true);
    assert(fs::is_dir(fs::join_path(BASE_DIR, "sub")) == true);
    assert(fs::is_dir(nested) == true);

    // mkdir_all on existing path should succeed (EEXIST is OK)
    rc = fs::mkdir_all(nested);
    assert(rc == 0);

    // Write a file in the nested dir
    fs::write_file(fs::join_path(nested, "file.txt"), "nested");
    assert(fs::is_file(fs::join_path(nested, "file.txt")) == true);

    // Cleanup
    fs::remove_file(fs::join_path(nested, "file.txt"));
    fs::rmdir(nested);
    fs::rmdir(fs::join_path(BASE_DIR, "sub"));
}

func test_is_dir_is_file(): void {
    assert(fs::is_dir(BASE_DIR) == true);
    assert(fs::is_file(BASE_DIR) == false);

    var file_path = fs::join_path(BASE_DIR, "hello.txt");
    fs::write_file(file_path, "hi");

    assert(fs::is_file(file_path) == true);
    assert(fs::is_dir(file_path) == false);

    // Non-existent path
    assert(fs::is_dir("/tmp/whist_fs_dir_test_nonexistent") == false);
    assert(fs::is_file("/tmp/whist_fs_dir_test_nonexistent") == false);

    fs::remove_file(file_path);
}

func test_cwd_chdir(): void {
    var original = fs::cwd();
    assert(original != "");

    var rc = fs::chdir(BASE_DIR);
    assert(rc == 0);

    var now = fs::cwd();
    // abs_path to normalize (e.g. /private/tmp on macOS)
    var abs_base = fs::abs_path(BASE_DIR);
    assert(now == abs_base);

    // Restore original directory
    fs::chdir(original);
}

func test_open_read_close_dir(): void {
    // Create some files to iterate
    fs::write_file(fs::join_path(BASE_DIR, "a.txt"), "a");
    fs::write_file(fs::join_path(BASE_DIR, "b.txt"), "b");
    fs::write_file(fs::join_path(BASE_DIR, "c.txt"), "c");

    var dh = fs::open_dir(BASE_DIR);
    assert(dh != null);

    var count: i64 = 0;
    var entry = fs::read_dir(dh);
    while (entry != "") {
        count = count + 1;
        entry = fs::read_dir(dh);
    }
    // Should have at least our 3 files (and no . or ..)
    assert(count >= 3);

    var rc = fs::close_dir(dh);
    assert(rc == 0);

    // open_dir on non-existent should return null
    var bad = fs::open_dir("/tmp/whist_fs_dir_nonexistent_xyz");
    assert(bad == null);

    // Cleanup
    fs::remove_file(fs::join_path(BASE_DIR, "a.txt"));
    fs::remove_file(fs::join_path(BASE_DIR, "b.txt"));
    fs::remove_file(fs::join_path(BASE_DIR, "c.txt"));
}

func test_join_path(): void {
    var p1 = fs::join_path("/tmp", "file.txt");
    assert(p1 == "/tmp/file.txt");

    // Trailing slash on first arg should be stripped
    var p2 = fs::join_path("/tmp/", "file.txt");
    assert(p2 == "/tmp/file.txt");

    // Leading slash on second arg should be stripped
    var p3 = fs::join_path("/tmp", "/file.txt");
    assert(p3 == "/tmp/file.txt");

    // Both
    var p4 = fs::join_path("/tmp/", "/file.txt");
    assert(p4 == "/tmp/file.txt");
}

func test_dirname(): void {
    var d1 = fs::dirname("/tmp/foo/bar.txt");
    assert(d1 == "/tmp/foo");

    var d2 = fs::dirname("/tmp/foo/");
    assert(d2 == "/tmp");

    var d3 = fs::dirname("file.txt");
    assert(d3 == ".");

    var d4 = fs::dirname("/");
    assert(d4 == "/");
}

func test_basename(): void {
    var b1 = fs::basename("/tmp/foo/bar.txt");
    assert(b1 == "bar.txt");

    var b2 = fs::basename("/tmp/foo/");
    assert(b2 == "foo");

    var b3 = fs::basename("file.txt");
    assert(b3 == "file.txt");
}

func test_extension(): void {
    var e1 = fs::extension("/tmp/foo/bar.txt");
    assert(e1 == ".txt");

    var e2 = fs::extension("/tmp/foo/bar.tar.gz");
    assert(e2 == ".gz");

    var e3 = fs::extension("/tmp/foo/bar");
    assert(e3 == "");

    // Dot in directory name shouldn't count
    var e4 = fs::extension("/tmp/foo.d/bar");
    assert(e4 == "");
}

func test_abs_path(): void {
    // abs_path of an existing dir should return non-empty
    var p = fs::abs_path(BASE_DIR);
    assert(p != "");

    // abs_path of non-existent should return empty
    var bad = fs::abs_path("/tmp/whist_fs_dir_nonexistent_xyz");
    assert(bad == "");
}

func test_modified_time(): void {
    var file_path = fs::join_path(BASE_DIR, "hello.txt");
    fs::write_file(file_path, "timestamp test");

    var mtime = fs::modified_time(file_path);
    assert(mtime > 0);

    // Non-existent file
    var bad = fs::modified_time("/tmp/whist_fs_dir_nonexistent_xyz.txt");
    assert(bad == -1);

    fs::remove_file(file_path);
}

func test_temp_dir(): void {
    var tmp = fs::temp_dir();
    assert(tmp != "");
    // Should be a directory
    assert(fs::is_dir(tmp) == true);
}

test "fs_dir_operations" {
    cleanup();

    test_mkdir_rmdir();
    // Re-create BASE_DIR for remaining tests
    fs::mkdir(BASE_DIR);

    test_mkdir_all();
    test_is_dir_is_file();
    test_cwd_chdir();
    test_open_read_close_dir();
    test_join_path();
    test_dirname();
    test_basename();
    test_extension();
    test_abs_path();
    test_modified_time();
    test_temp_dir();

    cleanup();
}

// --- fs_operations ---

func test_write_and_read(): void {
    var rc = fs::write_file("/tmp/whist_fs_ops_test.txt", "hello whist");
    assert(rc == 0);

    // Verify content was written by checking size
    var size = fs::file_size("/tmp/whist_fs_ops_test.txt");
    assert(size == 11);

    // Read it back to exercise the call
    fs::read_file("/tmp/whist_fs_ops_test.txt");
}

func test_append(): void {
    fs::write_file("/tmp/whist_fs_ops_append.txt", "first");
    var rc = fs::append_file("/tmp/whist_fs_ops_append.txt", " second");
    assert(rc == 0);

    var size = fs::file_size("/tmp/whist_fs_ops_append.txt");
    assert(size == 12);

    fs::remove_file("/tmp/whist_fs_ops_append.txt");
}

func test_file_exists(): void {
    fs::write_file("/tmp/whist_fs_ops_exists.txt", "x");
    assert(fs::file_exists("/tmp/whist_fs_ops_exists.txt") == true);

    fs::remove_file("/tmp/whist_fs_ops_exists.txt");
    assert(fs::file_exists("/tmp/whist_fs_ops_exists.txt") == false);
}

func test_rename(): void {
    fs::write_file("/tmp/whist_fs_ops_rename_a.txt", "data");
    var rc = fs::rename_file("/tmp/whist_fs_ops_rename_a.txt", "/tmp/whist_fs_ops_rename_b.txt");
    assert(rc == 0);
    assert(fs::file_exists("/tmp/whist_fs_ops_rename_a.txt") == false);
    assert(fs::file_exists("/tmp/whist_fs_ops_rename_b.txt") == true);

    fs::remove_file("/tmp/whist_fs_ops_rename_b.txt");
}

func test_remove(): void {
    fs::write_file("/tmp/whist_fs_ops_remove.txt", "gone");
    var rc = fs::remove_file("/tmp/whist_fs_ops_remove.txt");
    assert(rc == 0);
    assert(fs::file_exists("/tmp/whist_fs_ops_remove.txt") == false);
}

func test_handle_write_read(): void {
    // Write via handle
    var wh = fs::open("/tmp/whist_fs_ops_handle.txt", "w");
    assert(wh != null);

    var rc = fs::write_string(wh, "line one\n");
    assert(rc == 0);
    fs::write_string(wh, "line two\n");
    fs::flush(wh);
    fs::close(wh);

    // Read back via handle
    var rh = fs::open("/tmp/whist_fs_ops_handle.txt", "r");
    assert(rh != null);

    assert(fs::eof(rh) == false);

    // Read two lines, verifying via tell that position advances
    fs::read_line(rh);
    var pos_after_line1 = fs::tell(rh);
    assert(pos_after_line1 == 9);

    fs::read_line(rh);
    var pos_after_line2 = fs::tell(rh);
    assert(pos_after_line2 == 18);

    fs::close(rh);
    fs::remove_file("/tmp/whist_fs_ops_handle.txt");
}

func test_seek_tell(): void {
    fs::write_file("/tmp/whist_fs_ops_seek.txt", "abcdefghij");

    var h = fs::open("/tmp/whist_fs_ops_seek.txt", "r");
    assert(h != null);

    var pos0 = fs::tell(h);
    assert(pos0 == 0);

    // Read 5 bytes to advance position
    fs::read_line(h);
    var pos1 = fs::tell(h);
    assert(pos1 == 10);

    // Seek back to start
    var rc = fs::seek(h, 0, 0);
    assert(rc == 0);
    var pos2 = fs::tell(h);
    assert(pos2 == 0);

    // Seek to offset 5 from start
    fs::seek(h, 5, 0);
    var pos3 = fs::tell(h);
    assert(pos3 == 5);

    // Seek relative +2 from current
    fs::seek(h, 2, 1);
    var pos4 = fs::tell(h);
    assert(pos4 == 7);

    // Seek from end
    fs::seek(h, -3, 2);
    var pos5 = fs::tell(h);
    assert(pos5 == 7);

    fs::close(h);
    fs::remove_file("/tmp/whist_fs_ops_seek.txt");
}

func test_error_cases(): void {
    assert(fs::file_exists("/tmp/whist_fs_ops_nonexistent.txt") == false);

    var size = fs::file_size("/tmp/whist_fs_ops_nonexistent.txt");
    assert(size == -1);

    // Opening non-existent for read should return 0 (null handle)
    var h = fs::open("/tmp/whist_fs_ops_nonexistent.txt", "r");
    assert(h == null);

    // Remove on non-existent should fail
    var rc = fs::remove_file("/tmp/whist_fs_ops_nonexistent.txt");
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
    fs::remove_file("/tmp/whist_fs_ops_test.txt");
}
