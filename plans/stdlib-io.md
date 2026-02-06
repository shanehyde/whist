# Standard Library: File I/O

File system operations and streaming I/O.

## Overview

| Module | Purpose |
|--------|---------|
| `fs` | File operations (read, write, metadata) |
| `path` | Path manipulation |
| `io` | Streaming I/O traits and types |

## File Operations (fs module)

### Reading Files

```whist
import fs;

// Read entire file as string
var content = fs.read_to_string("config.txt")?;

// Read entire file as bytes
var bytes = fs.read("image.png")?;

// Read with options
var content = fs.read_to_string_with(
    "data.txt",
    ReadOptions { encoding: Encoding::Utf8 }
)?;
```

### Writing Files

```whist
// Write string to file
fs.write("output.txt", "Hello, World!")?;

// Write bytes
fs.write("data.bin", bytes)?;

// Append to file
fs.append("log.txt", "New log entry\n")?;

// Write with options
fs.write_with("output.txt", content, WriteOptions {
    create: true,
    truncate: true,
    permissions: 0o644,
})?;
```

### File Handles

For more control, use File handles:

```whist
// Open for reading
var file = fs.open("data.txt", OpenMode::Read)?;
var content = file.read_to_string()?;
file.close();

// Open for writing
var file = fs.create("output.txt")?;
file.write("Hello\n")?;
file.write("World\n")?;
file.close();

// Open with options
var file = fs.open_with("data.txt", OpenOptions {
    read: true,
    write: true,
    create: true,
    append: false,
    truncate: false,
})?;
```

### API

```whist
// fs module functions
func read(path: string): Result<Vec<u8>, IoError>;
func read_to_string(path: string): Result<string, IoError>;
func write(path: string, content: impl AsBytes): Result<void, IoError>;
func append(path: string, content: impl AsBytes): Result<void, IoError>;
func open(path: string, mode: OpenMode): Result<File, IoError>;
func create(path: string): Result<File, IoError>;

// File struct
struct File { ... }

impl File {
    // Reading
    func (File) read(buf: Span<u8>): Result<i64, IoError>;
    func (File) read_exact(buf: Span<u8>): Result<void, IoError>;
    func (File) read_to_end(): Result<Vec<u8>, IoError>;
    func (File) read_to_string(): Result<string, IoError>;
    func (File) read_line(): Result<string, IoError>;

    // Writing
    func (File) write(data: Span<u8>): Result<i64, IoError>;
    func (File) write_all(data: Span<u8>): Result<void, IoError>;
    func (File) flush(): Result<void, IoError>;

    // Seeking
    func (File) seek(pos: SeekFrom): Result<i64, IoError>;
    func (File) position(): Result<i64, IoError>;

    // Metadata
    func (File) metadata(): Result<Metadata, IoError>;
    func (File) set_permissions(perm: Permissions): Result<void, IoError>;

    // Close
    func (File) close(): void;
}

enum OpenMode {
    Read,
    Write,
    Append,
    ReadWrite,
}

enum SeekFrom {
    Start(i64),
    End(i64),
    Current(i64),
}
```

## Directory Operations

```whist
// Create directory
fs.create_dir("new_folder")?;
fs.create_dir_all("path/to/nested/folder")?;

// Remove
fs.remove_file("old.txt")?;
fs.remove_dir("empty_folder")?;
fs.remove_dir_all("folder_with_contents")?;

// List directory
var entries = fs.read_dir("src")?;
foreach entry in entries {
    print("{entry.name}: {entry.file_type}\n");
}

// Copy/Move
fs.copy("src.txt", "dst.txt")?;
fs.rename("old.txt", "new.txt")?;

// Check existence
if fs.exists("config.txt") {
    // ...
}
```

### API

```whist
func create_dir(path: string): Result<void, IoError>;
func create_dir_all(path: string): Result<void, IoError>;
func remove_file(path: string): Result<void, IoError>;
func remove_dir(path: string): Result<void, IoError>;
func remove_dir_all(path: string): Result<void, IoError>;
func read_dir(path: string): Result<Vec<DirEntry>, IoError>;
func copy(from: string, to: string): Result<void, IoError>;
func rename(from: string, to: string): Result<void, IoError>;
func exists(path: string): bool;

struct DirEntry {
    name: string,
    path: string,
    file_type: FileType,
}

enum FileType {
    File,
    Directory,
    Symlink,
}
```

## File Metadata

```whist
var meta = fs.metadata("file.txt")?;
print("Size: {meta.size} bytes\n");
print("Modified: {meta.modified}\n");
print("Is directory: {meta.is_dir}\n");
print("Permissions: {meta.permissions}\n");

// Symlink metadata
var link_meta = fs.symlink_metadata("link")?;
print("Is symlink: {link_meta.is_symlink}\n");
```

### API

```whist
func metadata(path: string): Result<Metadata, IoError>;
func symlink_metadata(path: string): Result<Metadata, IoError>;
func set_permissions(path: string, perm: Permissions): Result<void, IoError>;

struct Metadata {
    size: i64,
    is_file: bool,
    is_dir: bool,
    is_symlink: bool,
    permissions: Permissions,
    modified: Time,
    accessed: Time,
    created: ?Time,  // Not available on all platforms
}

struct Permissions {
    readonly: bool,
    mode: i32,  // Unix mode bits
}
```

## Path Manipulation (path module)

```whist
import path;

var p = path.join("src", "main.w");       // "src/main.w"
var dir = path.dirname("src/main.w");      // "src"
var name = path.basename("src/main.w");    // "main.w"
var ext = path.extension("main.w");        // "w"
var stem = path.stem("main.w");            // "main"

var abs = path.absolute("./file.txt")?;    // "/home/user/project/file.txt"
var rel = path.relative("/home/user", "/home/user/docs")?;  // "docs"

var normalized = path.normalize("./foo/../bar/./baz");  // "bar/baz"

if path.is_absolute("/usr/bin") { ... }
if path.exists("config.txt") { ... }
```

### Path Type

```whist
struct Path {
    inner: string,
}

impl Path {
    func from(s: string): Path;
    func to_string(): string;

    func join(other: string): Path;
    func parent(): ?Path;
    func file_name(): ?string;
    func extension(): ?string;
    func stem(): ?string;

    func is_absolute(): bool;
    func is_relative(): bool;

    func exists(): bool;
    func is_file(): bool;
    func is_dir(): bool;

    func components(): Iterator<string>;
}
```

## Buffered I/O

For efficiency with many small reads/writes:

```whist
import io;

// Buffered reader
var file = fs.open("large.txt", OpenMode::Read)?;
var reader = io.BufReader::new(file);
foreach line in reader.lines() {
    process(line?);
}

// Buffered writer
var file = fs.create("output.txt")?;
var writer = io.BufWriter::new(file);
writer.write("Line 1\n")?;
writer.write("Line 2\n")?;
writer.flush()?;  // Important: flush before close
```

### API

```whist
struct BufReader<R: Read> {
    inner: R,
    buf: Vec<u8>,
    pos: i64,
}

impl<R: Read> BufReader<R> {
    func new(inner: R): BufReader<R>;
    func with_capacity(cap: i64, inner: R): BufReader<R>;
    func lines(): LinesIterator<R>;
    func read_line(): Result<string, IoError>;
}

struct BufWriter<W: Write> {
    inner: W,
    buf: Vec<u8>,
}

impl<W: Write> BufWriter<W> {
    func new(inner: W): BufWriter<W>;
    func with_capacity(cap: i64, inner: W): BufWriter<W>;
    func flush(): Result<void, IoError>;
}
```

## I/O Traits

```whist
trait Read {
    func read(buf: Span<u8>): Result<i64, IoError>;

    // Provided
    func read_exact(buf: Span<u8>): Result<void, IoError>;
    func read_to_end(): Result<Vec<u8>, IoError>;
    func read_to_string(): Result<string, IoError>;
}

trait Write {
    func write(buf: Span<u8>): Result<i64, IoError>;
    func flush(): Result<void, IoError>;

    // Provided
    func write_all(buf: Span<u8>): Result<void, IoError>;
}

trait Seek {
    func seek(pos: SeekFrom): Result<i64, IoError>;

    // Provided
    func rewind(): Result<void, IoError>;
    func position(): Result<i64, IoError>;
}

trait BufRead: Read {
    func fill_buf(): Result<Span<u8>, IoError>;
    func consume(amt: i64): void;

    // Provided
    func read_line(): Result<string, IoError>;
    func lines(): LinesIterator;
}
```

## Standard Streams

```whist
import io;

// Standard input
var line = io.stdin.read_line()?;

// Standard output
io.stdout.write("Hello\n")?;
io.stdout.flush()?;

// Standard error
io.stderr.write("Error: something went wrong\n")?;

// Shortcuts
print("Hello, World!\n");      // stdout
eprint("Warning: ...\n");      // stderr
```

## Error Handling

```whist
enum IoError {
    NotFound,
    PermissionDenied,
    AlreadyExists,
    InvalidInput,
    InvalidData,
    TimedOut,
    Interrupted,
    UnexpectedEof,
    Other(string),
}

impl IoError {
    func kind(): IoErrorKind;
    func message(): string;
}

// Usage
match fs.read_to_string("config.txt") {
    Ok(content) => process(content),
    Err(IoError::NotFound) => use_defaults(),
    Err(IoError::PermissionDenied) => panic("Cannot read config"),
    Err(e) => return Err(e),
}
```

## Temporary Files

```whist
import fs.temp;

// Create temp file
var file = temp.create_file()?;
file.write("temporary data")?;
// File deleted when `file` is dropped

// Create temp file with prefix
var file = temp.create_file_in(temp.dir(), "myapp_")?;

// Temp directory
var dir = temp.create_dir()?;
// Directory deleted when `dir` is dropped

// Get system temp directory
var tmp = temp.dir();  // e.g., "/tmp" or "C:\Temp"
```

## File Watching

```whist
import fs.watch;

var watcher = watch.Watcher::new()?;
watcher.watch("src", WatchMode::Recursive)?;

foreach event in watcher.events() {
    match event {
        FileCreated(path) => print("Created: {path}\n"),
        FileModified(path) => print("Modified: {path}\n"),
        FileDeleted(path) => print("Deleted: {path}\n"),
    }
}
```

## Examples

### Read Lines from File

```whist
import fs;
import io;

func count_lines(path: string): Result<i64, IoError> {
    var file = fs.open(path, OpenMode::Read)?;
    var reader = io.BufReader::new(file);

    var count = 0;
    foreach line in reader.lines() {
        line?;  // Propagate any read errors
        count += 1;
    }

    return Ok(count);
}
```

### Copy File with Progress

```whist
func copy_with_progress(src: string, dst: string): Result<void, IoError> {
    var meta = fs.metadata(src)?;
    var total = meta.size;
    var copied = 0;

    var reader = fs.open(src, OpenMode::Read)?;
    var writer = fs.create(dst)?;
    var buf = [0u8; 8192];

    loop {
        var n = reader.read(buf)?;
        if n == 0 { break; }

        writer.write_all(buf[0:n])?;
        copied += n;

        var pct = (copied * 100) / total;
        print("\rCopying: {pct}%");
    }

    print("\nDone!\n");
    return Ok(());
}
```

### Process Config File

```whist
func load_config(path: string): Result<Config, Error> {
    var content = fs.read_to_string(path)
        .map_err(|e| Error::Io(e))?;

    var config = parse_config(content)
        .map_err(|e| Error::Parse(e))?;

    return Ok(config);
}
```

## Open Questions

1. **Async I/O?**
   - Sync initially
   - Add async later with async/await

2. **Text encoding?**
   - UTF-8 by default
   - Support other encodings?

3. **File locking?**
   - Advisory locks
   - Platform differences

4. **Memory mapping?**
   - mmap for large files
   - Safety concerns

## Related Features

- [Result/Option](result-option.md) - Error handling
- [Traits](traits.md) - Read, Write, Seek traits
- [Closures](closures.md) - Callbacks for async I/O
