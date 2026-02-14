// Expected: PASS: fs_dir_operations
// Test fs module: directory operations, path utilities, metadata

import fs;
import std;

const BASE_DIR = "/tmp/whist_fs_dir_test";

func cleanup(): void {
    // Remove any files/dirs from previous runs
    fs.remove_file(fs.join_path(BASE_DIR, "sub/nested/file.txt"));
    fs.rmdir(fs.join_path(BASE_DIR, "sub/nested"));
    fs.rmdir(fs.join_path(BASE_DIR, "sub"));
    fs.remove_file(fs.join_path(BASE_DIR, "hello.txt"));
    fs.remove_file(fs.join_path(BASE_DIR, "a.txt"));
    fs.remove_file(fs.join_path(BASE_DIR, "b.txt"));
    fs.remove_file(fs.join_path(BASE_DIR, "c.txt"));
    fs.rmdir(BASE_DIR);
}

func test_mkdir_rmdir(): void {
    var rc = fs.mkdir(BASE_DIR);
    assert(rc == 0);
    assert(fs.is_dir(BASE_DIR) == true);

    // mkdir on existing dir should fail
    rc = fs.mkdir(BASE_DIR);
    assert(rc == -1);

    // rmdir on empty dir
    rc = fs.rmdir(BASE_DIR);
    assert(rc == 0);
    assert(fs.is_dir(BASE_DIR) == false);

    // rmdir on non-existent should fail
    rc = fs.rmdir(BASE_DIR);
    assert(rc == -1);
}

func test_mkdir_all(): void {
    var nested = fs.join_path(BASE_DIR, "sub/nested");
    var rc = fs.mkdir_all(nested);
    assert(rc == 0);
    assert(fs.is_dir(BASE_DIR) == true);
    assert(fs.is_dir(fs.join_path(BASE_DIR, "sub")) == true);
    assert(fs.is_dir(nested) == true);

    // mkdir_all on existing path should succeed (EEXIST is OK)
    rc = fs.mkdir_all(nested);
    assert(rc == 0);

    // Write a file in the nested dir
    fs.write_file(fs.join_path(nested, "file.txt"), "nested");
    assert(fs.is_file(fs.join_path(nested, "file.txt")) == true);

    // Cleanup
    fs.remove_file(fs.join_path(nested, "file.txt"));
    fs.rmdir(nested);
    fs.rmdir(fs.join_path(BASE_DIR, "sub"));
}

func test_is_dir_is_file(): void {
    assert(fs.is_dir(BASE_DIR) == true);
    assert(fs.is_file(BASE_DIR) == false);

    var file_path = fs.join_path(BASE_DIR, "hello.txt");
    fs.write_file(file_path, "hi");

    assert(fs.is_file(file_path) == true);
    assert(fs.is_dir(file_path) == false);

    // Non-existent path
    assert(fs.is_dir("/tmp/whist_fs_dir_test_nonexistent") == false);
    assert(fs.is_file("/tmp/whist_fs_dir_test_nonexistent") == false);

    fs.remove_file(file_path);
}

func test_cwd_chdir(): void {
    var original = fs.cwd();
    assert(original != "");

    var rc = fs.chdir(BASE_DIR);
    assert(rc == 0);

    var now = fs.cwd();
    // abs_path to normalize (e.g. /private/tmp on macOS)
    var abs_base = fs.abs_path(BASE_DIR);
    assert(now == abs_base);

    // Restore original directory
    fs.chdir(original);
}

func test_open_read_close_dir(): void {
    // Create some files to iterate
    fs.write_file(fs.join_path(BASE_DIR, "a.txt"), "a");
    fs.write_file(fs.join_path(BASE_DIR, "b.txt"), "b");
    fs.write_file(fs.join_path(BASE_DIR, "c.txt"), "c");

    var dh = fs.open_dir(BASE_DIR);
    assert(dh != null);

    var count: i64 = 0;
    var entry = fs.read_dir(dh);
    while (entry != "") {
        count = count + 1;
        entry = fs.read_dir(dh);
    }
    // Should have at least our 3 files (and no . or ..)
    assert(count >= 3);

    var rc = fs.close_dir(dh);
    assert(rc == 0);

    // open_dir on non-existent should return null
    var bad = fs.open_dir("/tmp/whist_fs_dir_nonexistent_xyz");
    assert(bad == null);

    // Cleanup
    fs.remove_file(fs.join_path(BASE_DIR, "a.txt"));
    fs.remove_file(fs.join_path(BASE_DIR, "b.txt"));
    fs.remove_file(fs.join_path(BASE_DIR, "c.txt"));
}

func test_join_path(): void {
    var p1 = fs.join_path("/tmp", "file.txt");
    assert(p1 == "/tmp/file.txt");

    // Trailing slash on first arg should be stripped
    var p2 = fs.join_path("/tmp/", "file.txt");
    assert(p2 == "/tmp/file.txt");

    // Leading slash on second arg should be stripped
    var p3 = fs.join_path("/tmp", "/file.txt");
    assert(p3 == "/tmp/file.txt");

    // Both
    var p4 = fs.join_path("/tmp/", "/file.txt");
    assert(p4 == "/tmp/file.txt");
}

func test_dirname(): void {
    var d1 = fs.dirname("/tmp/foo/bar.txt");
    assert(d1 == "/tmp/foo");

    var d2 = fs.dirname("/tmp/foo/");
    assert(d2 == "/tmp");

    var d3 = fs.dirname("file.txt");
    assert(d3 == ".");

    var d4 = fs.dirname("/");
    assert(d4 == "/");
}

func test_basename(): void {
    var b1 = fs.basename("/tmp/foo/bar.txt");
    assert(b1 == "bar.txt");

    var b2 = fs.basename("/tmp/foo/");
    assert(b2 == "foo");

    var b3 = fs.basename("file.txt");
    assert(b3 == "file.txt");
}

func test_extension(): void {
    var e1 = fs.extension("/tmp/foo/bar.txt");
    assert(e1 == ".txt");

    var e2 = fs.extension("/tmp/foo/bar.tar.gz");
    assert(e2 == ".gz");

    var e3 = fs.extension("/tmp/foo/bar");
    assert(e3 == "");

    // Dot in directory name shouldn't count
    var e4 = fs.extension("/tmp/foo.d/bar");
    assert(e4 == "");
}

func test_abs_path(): void {
    // abs_path of an existing dir should return non-empty
    var p = fs.abs_path(BASE_DIR);
    assert(p != "");

    // abs_path of non-existent should return empty
    var bad = fs.abs_path("/tmp/whist_fs_dir_nonexistent_xyz");
    assert(bad == "");
}

func test_modified_time(): void {
    var file_path = fs.join_path(BASE_DIR, "hello.txt");
    fs.write_file(file_path, "timestamp test");

    var mtime = fs.modified_time(file_path);
    assert(mtime > 0);

    // Non-existent file
    var bad = fs.modified_time("/tmp/whist_fs_dir_nonexistent_xyz.txt");
    assert(bad == -1);

    fs.remove_file(file_path);
}

func test_temp_dir(): void {
    var tmp = fs.temp_dir();
    assert(tmp != "");
    // Should be a directory
    assert(fs.is_dir(tmp) == true);
}

test "fs_dir_operations" {
    cleanup();

    test_mkdir_rmdir();
    // Re-create BASE_DIR for remaining tests
    fs.mkdir(BASE_DIR);

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
