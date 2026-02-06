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
    func fs__read_file(path: string): string;
    func fs__write_file(path: string, content: string): i32;
    func fs__append_file(path: string, content: string): i32;
    func fs__file_exists(path: string): bool;
    func fs__remove_file(path: string): i32;
    func fs__rename_file(old_path: string, new_path: string): i32;
    func fs__file_size(path: string): i64;

    // Handle-based API
    func fs__open(path: string, mode: string): u64;
    func fs__close(handle: u64): i32;
    func fs__read_line(handle: u64): string;
    func fs__write_string(handle: u64, content: string): i32;
    func fs__flush(handle: u64): i32;
    func fs__seek(handle: u64, offset: i64, whence: i32): i32;
    func fs__tell(handle: u64): i64;
    func fs__eof(handle: u64): bool;
}

// Convenience API

func read_file(path: string): string {
    return fs__read_file(path);
}

func write_file(path: string, content: string): i32 {
    return fs__write_file(path, content);
}

func append_file(path: string, content: string): i32 {
    return fs__append_file(path, content);
}

func file_exists(path: string): bool {
    return fs__file_exists(path);
}

func remove_file(path: string): i32 {
    return fs__remove_file(path);
}

func rename_file(old_path: string, new_path: string): i32 {
    return fs__rename_file(old_path, new_path);
}

func file_size(path: string): i64 {
    return fs__file_size(path);
}

// Handle-based API

func open(path: string, mode: string): u64 {
    return fs__open(path, mode);
}

func close(handle: u64): i32 {
    return fs__close(handle);
}

func read_line(handle: u64): string {
    return fs__read_line(handle);
}

func write_string(handle: u64, content: string): i32 {
    return fs__write_string(handle, content);
}

func flush(handle: u64): i32 {
    return fs__flush(handle);
}

func seek(handle: u64, offset: i64, whence: i32): i32 {
    return fs__seek(handle, offset, whence);
}

func tell(handle: u64): i64 {
    return fs__tell(handle);
}

func eof(handle: u64): bool {
    return fs__eof(handle);
}
