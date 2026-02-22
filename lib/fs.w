// File services for Whist
//
// Usage:
//   import fs;
//   var content = fs.read_file("config.txt");
//   fs.write_file("output.txt", "hello");
//
// Compile with: bin/w0 program.w | cc -x c -Ilib/include -o program -

private extern fs {
    // Convenience API (no handles)
    func fs__read_file(path: string) -> string;
    func fs__write_file(path: string, content: string) -> i32;
    func fs__append_file(path: string, content: string) -> i32;
    func fs__file_exists(path: string) -> bool;
    func fs__remove_file(path: string) -> i32;
    func fs__rename_file(old_path: string, new_path: string) -> i32;
    func fs__file_size(path: string) -> i64;

    // Handle-based API
    func fs__open(path: string, mode: string) -> voidptr;
    func fs__close(handle: voidptr) -> i32;
    func fs__read_line(handle: voidptr) -> string;
    func fs__write_string(handle: voidptr, content: string) -> i32;
    func fs__flush(handle: voidptr) -> i32;
    func fs__seek(handle: voidptr, offset: i64, whence: i32) -> i32;
    func fs__tell(handle: voidptr) -> i64;
    func fs__eof(handle: voidptr) -> bool;

    // Directory operations
    func fs__mkdir(path: string) -> i32;
    func fs__mkdir_all(path: string) -> i32;
    func fs__rmdir(path: string) -> i32;
    func fs__is_dir(path: string) -> bool;
    func fs__is_file(path: string) -> bool;
    func fs__cwd() -> string;
    func fs__chdir(path: string) -> i32;

    // Directory iteration (handle-based)
    func fs__open_dir(path: string) -> voidptr;
    func fs__read_dir(handle: voidptr) -> string;
    func fs__close_dir(handle: voidptr) -> i32;

    // Path utilities
    func fs__join_path(a: string, b: string) -> string;
    func fs__dirname(path: string) -> string;
    func fs__basename(path: string) -> string;
    func fs__extension(path: string) -> string;
    func fs__abs_path(path: string) -> string;

    // Metadata & temp
    func fs__modified_time(path: string) -> i64;
    func fs__temp_dir() -> string;
}

// Convenience API

public func read_file(path: string) -> string {
    return fs__read_file(path);
}

public func write_file(path: string, content: string) -> i32 {
    return fs__write_file(path, content);
}

public func append_file(path: string, content: string) -> i32 {
    return fs__append_file(path, content);
}

public func file_exists(path: string) -> bool {
    return fs__file_exists(path);
}

public func remove_file(path: string) -> i32 {
    return fs__remove_file(path);
}

public func rename_file(old_path: string, new_path: string) -> i32 {
    return fs__rename_file(old_path, new_path);
}

public func file_size(path: string) -> i64 {
    return fs__file_size(path);
}

// Handle-based API

public func open(path: string, mode: string) -> voidptr {
    return fs__open(path, mode);
}

public func close(handle: voidptr) -> i32 {
    return fs__close(handle);
}

public func read_line(handle: voidptr) -> string {
    return fs__read_line(handle);
}

public func write_string(handle: voidptr, content: string) -> i32 {
    return fs__write_string(handle, content);
}

public func flush(handle: voidptr) -> i32 {
    return fs__flush(handle);
}

public func seek(handle: voidptr, offset: i64, whence: i32) -> i32 {
    return fs__seek(handle, offset, whence);
}

public func tell(handle: voidptr) -> i64 {
    return fs__tell(handle);
}

public func eof(handle: voidptr) -> bool {
    return fs__eof(handle);
}

// Directory operations

public func mkdir(path: string) -> i32 {
    return fs__mkdir(path);
}

public func mkdir_all(path: string) -> i32 {
    return fs__mkdir_all(path);
}

public func rmdir(path: string) -> i32 {
    return fs__rmdir(path);
}

public func is_dir(path: string) -> bool {
    return fs__is_dir(path);
}

public func is_file(path: string) -> bool {
    return fs__is_file(path);
}

public func cwd() -> string {
    return fs__cwd();
}

public func chdir(path: string) -> i32 {
    return fs__chdir(path);
}

// Directory iteration (handle-based)

public func open_dir(path: string) -> voidptr {
    return fs__open_dir(path);
}

public func read_dir(handle: voidptr) -> string {
    return fs__read_dir(handle);
}

public func close_dir(handle: voidptr) -> i32 {
    return fs__close_dir(handle);
}

// Path utilities

public func join_path(a: string, b: string) -> string {
    return fs__join_path(a, b);
}

public func dirname(path: string) -> string {
    return fs__dirname(path);
}

public func basename(path: string) -> string {
    return fs__basename(path);
}

public func extension(path: string) -> string {
    return fs__extension(path);
}

public func abs_path(path: string) -> string {
    return fs__abs_path(path);
}

// Metadata & temp

public func modified_time(path: string) -> i64 {
    return fs__modified_time(path);
}

public func temp_dir() -> string {
    return fs__temp_dir();
}

// Recursive directory walk

public func walk_dir(dir: string) -> Vec<string> {
    var files = new Vec<string>{};
    var dirs = new Vec<string>{};
    var dh = fs__open_dir(dir);
    if (dh == null) {
        return files;
    }
    var entry = fs__read_dir(dh);
    while (entry != "") {
        var path = fs__join_path(dir, entry);
        if (fs__is_dir(path)) {
            dirs.push(path);
        } else {
            files.push(path);
        }
        entry = fs__read_dir(dh);
    }
    fs__close_dir(dh);
    foreach (const subdir in dirs) {
        var sub_files = walk_dir(subdir);
        files.extend(sub_files);
    }
    return files;
}
