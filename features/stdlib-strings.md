# Standard Library: String Manipulation

String operations, formatting, and Unicode support.

## Overview

| Feature | Description |
|---------|-------------|
| Core methods | Length, access, slicing |
| Searching | Find, contains, starts/ends with |
| Transformation | Case, trim, replace |
| Splitting/Joining | Split, join, lines |
| Formatting | String interpolation, format functions |
| Unicode | Grapheme clusters, normalization |

## String Type

```whist
// String is a built-in type
// Internally: UTF-8 encoded, length-prefixed

var s: string = "Hello, World!";
var empty: string = "";
var multiline: string = "Line 1\nLine 2\nLine 3";
```

## Core Methods

```whist
impl string {
    // Length
    func (string) len() -> i64;           // Byte length
    func (string) chars() -> i64;         // Character count
    func (string) is_empty() -> bool;

    // Access
    func (string) char_at(index: i64) -> ?char;
    func (string) byte_at(index: i64) -> ?u8;

    // Slicing
    func (string) slice(start: i64, end: i64) -> string;
    func (string) substring(start: i64, len: i64) -> string;

    // Comparison
    func (string) eq(other: string) -> bool;
    func (string) eq_ignore_case(other: string) -> bool;
    func (string) cmp(other: string) -> Ordering;
}
```

### Usage

```whist
var s = "Hello";

print(s.len());         // 5
print(s.chars());       // 5
print(s.char_at(0));    // Some('H')
print(s.slice(0, 4));   // "Hell"
print(s.is_empty());    // false

// Unicode
var emoji = "Hello 👋";
print(emoji.len());     // 10 (bytes)
print(emoji.chars());   // 7 (characters)
```

## Searching

```whist
impl string {
    // Contains
    func (string) contains(needle: string) -> bool;
    func (string) contains_char(c: char) -> bool;

    // Position
    func (string) find(needle: string) -> ?i64;
    func (string) rfind(needle: string) -> ?i64;
    func (string) find_char(c: char) -> ?i64;

    // Prefix/Suffix
    func (string) starts_with(prefix: string) -> bool;
    func (string) ends_with(suffix: string) -> bool;

    // Count
    func (string) count(needle: string) -> i64;
}
```

### Usage

```whist
var s = "Hello, World!";

print(s.contains("World"));     // true
print(s.find("o"));             // Some(4)
print(s.rfind("o"));            // Some(8)
print(s.starts_with("Hello"));  // true
print(s.ends_with("!"));        // true
print(s.count("l"));            // 3
```

## Transformation

```whist
impl string {
    // Case
    func (string) to_upper() -> string;
    func (string) to_lower() -> string;
    func (string) to_title() -> string;     // Title Case
    func (string) capitalize() -> string;   // First char upper

    // Trimming
    func (string) trim() -> string;
    func (string) trim_start() -> string;
    func (string) trim_end() -> string;
    func (string) trim_chars(chars: string) -> string;

    // Padding
    func (string) pad_start(len: i64, c: char) -> string;
    func (string) pad_end(len: i64, c: char) -> string;
    func (string) center(len: i64, c: char) -> string;

    // Replace
    func (string) replace(from: string, to: string) -> string;
    func (string) replace_first(from: string, to: string) -> string;
    func (string) replace_n(from: string, to: string, n: i64) -> string;

    // Remove
    func (string) remove_prefix(prefix: string) -> string;
    func (string) remove_suffix(suffix: string) -> string;

    // Repeat
    func (string) repeat(n: i64) -> string;

    // Reverse
    func (string) reverse() -> string;
}
```

### Usage

```whist
var s = "  hello world  ";

print(s.trim());                    // "hello world"
print(s.trim().to_upper());         // "HELLO WORLD"
print(s.trim().to_title());         // "Hello World"
print(s.trim().capitalize());       // "Hello world"

print("42".pad_start(5, '0'));      // "00042"
print("hi".pad_end(5, ' '));        // "hi   "
print("x".center(5, '-'));          // "--x--"

print("aaa".replace("a", "b"));     // "bbb"
print("hello".repeat(3));           // "hellohellohello"
print("hello".reverse());           // "olleh"
```

## Splitting and Joining

```whist
impl string {
    // Split
    func (string) split(sep: string) -> Vec<string>;
    func (string) split_n(sep: string, n: i64) -> Vec<string>;
    func (string) split_whitespace() -> Vec<string>;
    func (string) lines() -> Vec<string>;

    // Split (lazy iterator versions)
    func (string) split_iter(sep: string) -> SplitIterator;
    func (string) lines_iter() -> LinesIterator;

    // Join (on Vec<string>)
    // Note: This is on the collection, not on string
}

impl Vec<string> {
    func (Vec<string>) join(sep: string) -> string;
}
```

### Usage

```whist
var csv = "a,b,c,d";
var parts = csv.split(",");         // ["a", "b", "c", "d"]

var text = "hello   world  foo";
var words = text.split_whitespace(); // ["hello", "world", "foo"]

var multiline = "line1\nline2\nline3";
var lines = multiline.lines();       // ["line1", "line2", "line3"]

var joined = ["a", "b", "c"].join("-"); // "a-b-c"

// Lazy iteration
foreach line in text.lines_iter() {
    process(line);
}
```

## Characters and Bytes

```whist
impl string {
    // Iteration
    func (string) chars_iter() -> CharIterator;
    func (string) bytes_iter() -> ByteIterator;
    func (string) char_indices() -> CharIndexIterator;

    // Conversion
    func (string) as_bytes() -> Span<u8>;
    func from_utf8(bytes: Span<u8>) -> Result<string, Utf8Error>;
    func from_utf8_lossy(bytes: Span<u8>) -> string;
}

impl char {
    func (char) to_string() -> string;
    func (char) is_alphabetic() -> bool;
    func (char) is_numeric() -> bool;
    func (char) is_alphanumeric() -> bool;
    func (char) is_whitespace() -> bool;
    func (char) is_uppercase() -> bool;
    func (char) is_lowercase() -> bool;
    func (char) to_uppercase() -> char;
    func (char) to_lowercase() -> char;
}
```

### Usage

```whist
var s = "Hello";

foreach c in s.chars_iter() {
    print("{c}\n");
}

foreach (i, c) in s.char_indices() {
    print("{i}: {c}\n");
}

var bytes = s.as_bytes();  // [72, 101, 108, 108, 111]
var back = string.from_utf8(bytes)?;
```

## Parsing

```whist
impl string {
    func (string) parse_i64() -> Result<i64, ParseError>;
    func (string) parse_f64() -> Result<f64, ParseError>;
    func (string) parse_bool() -> Result<bool, ParseError>;
    func (string) parse<T: FromStr>() -> Result<T, T::Error>;
}
```

### Usage

```whist
var n = "42".parse_i64()?;           // 42
var f = "3.14".parse_f64()?;         // 3.14
var b = "true".parse_bool()?;        // true

// Generic parsing
var point = "10,20".parse::<Point>()?;
```

## String Conversion

```whist
trait ToString {
    func to_string() -> string;
}

// Built-in implementations
impl ToString for i64 { ... }
impl ToString for f64 { ... }
impl ToString for bool { ... }
impl ToString for char { ... }

// Usage
var n: i64 = 42;
var s = n.to_string();  // "42"

var f: f64 = 3.14159;
var s = f.to_string();  // "3.14159"
```

## StringBuilder

For efficient string concatenation:

```whist
struct StringBuilder {
    // Internal buffer
}

impl StringBuilder {
    func new() -> StringBuilder;
    func with_capacity(cap: i64) -> StringBuilder;

    func (StringBuilder) append(s: string) -> void;
    func (StringBuilder) append_char(c: char) -> void;
    func (StringBuilder) append_line(s: string) -> void;
    func (StringBuilder) append_format(fmt: string, args: ...) -> void;

    func (StringBuilder) len() -> i64;
    func (StringBuilder) capacity() -> i64;
    func (StringBuilder) clear() -> void;

    func (StringBuilder) to_string() -> string;
}
```

### Usage

```whist
var sb = StringBuilder::new();
sb.append("Hello");
sb.append(", ");
sb.append("World");
sb.append_char('!');

var result = sb.to_string();  // "Hello, World!"

// Efficient for loops
var sb = StringBuilder::with_capacity(1000);
foreach i in 0..100 {
    sb.append_line("Line {i}");
}
```

## Formatting

### String Interpolation

See [String Interpolation](string-interpolation.md) for full details.

```whist
var name = "Alice";
var age = 30;
print("Name: {name}, Age: {age}");

// With format specifiers
var pi = 3.14159;
print("Pi: {pi:.2}");  // "Pi: 3.14"
```

### Format Function

```whist
func format(template: string, args: ...) -> string;

var s = format("Hello, {}!", "World");
var s = format("{} + {} = {}", 1, 2, 3);
var s = format("{name} is {age} years old", name: "Bob", age: 25);
```

## Unicode Support

### Grapheme Clusters

Handle user-perceived characters correctly:

```whist
import unicode;

var emoji = "👨‍👩‍👧‍👦";  // Family emoji (multiple code points)

print(emoji.len());                     // 25 (bytes)
print(emoji.chars());                   // 7 (code points)
print(unicode.graphemes(emoji).count()); // 1 (grapheme cluster)

foreach g in unicode.graphemes(text) {
    print("Grapheme: {g}\n");
}
```

### Normalization

```whist
import unicode;

var s1 = "café";   // 'é' as single character
var s2 = "café";   // 'e' + combining acute

print(s1 == s2);   // false (different bytes)
print(unicode.normalize_nfc(s1) == unicode.normalize_nfc(s2));  // true

// Normalization forms
var nfc = unicode.normalize_nfc(s);   // Composed
var nfd = unicode.normalize_nfd(s);   // Decomposed
var nfkc = unicode.normalize_nfkc(s); // Compatibility composed
var nfkd = unicode.normalize_nfkd(s); // Compatibility decomposed
```

### Case Folding

For case-insensitive comparison:

```whist
import unicode;

var s1 = "HELLO";
var s2 = "hello";

// Simple ASCII comparison
print(s1.to_lower() == s2);  // true

// Full Unicode case folding
print(unicode.case_fold(s1) == unicode.case_fold(s2));  // true

// Works for special cases
print(unicode.case_fold("ß") == unicode.case_fold("ss"));  // true
```

## Regular Expressions

```whist
import regex;

var re = regex.compile(r"\d+")?;

// Match
if re.is_match("hello123world") {
    print("Found digits!\n");
}

// Find
if let Some(m) = re.find("hello123world") {
    print("Found: {m.as_str()} at {m.start()}\n");
}

// Find all
foreach m in re.find_all("a1b2c3") {
    print("Match: {m.as_str()}\n");  // 1, 2, 3
}

// Capture groups
var re = regex.compile(r"(\w+)@(\w+)\.(\w+)")?;
if let Some(caps) = re.captures("user@example.com") {
    print("User: {caps.get(1)}\n");    // user
    print("Domain: {caps.get(2)}\n");  // example
    print("TLD: {caps.get(3)}\n");     // com
}

// Replace
var result = re.replace_all("a1b2c3", "X");  // "aXbXcX"
```

## Examples

### Parse CSV Line

```whist
func parse_csv_line(line: string) -> Vec<string> {
    var fields = Vec::new();
    var current = StringBuilder::new();
    var in_quotes = false;

    foreach c in line.chars_iter() {
        match c {
            '"' => in_quotes = !in_quotes,
            ',' if !in_quotes => {
                fields.push(current.to_string().trim());
                current.clear();
            },
            _ => current.append_char(c),
        }
    }

    fields.push(current.to_string().trim());
    return fields;
}
```

### Word Count

```whist
func word_count(text: string) -> HashMap<string, i64> {
    var counts = HashMap::new();

    foreach word in text.split_whitespace() {
        var normalized = word.to_lower().trim_chars(".,!?");
        var count = counts.entry(normalized).or_insert(0);
        *count += 1;
    }

    return counts;
}
```

### Template Engine

```whist
func render_template(template: string, vars: HashMap<string, string>) -> string {
    var result = template;

    foreach (key, value) in vars {
        result = result.replace("{{" + key + "}}", value);
    }

    return result;
}

var html = render_template(
    "<h1>{{title}}</h1><p>{{content}}</p>",
    HashMap::from([
        ("title", "Hello"),
        ("content", "Welcome to my site!"),
    ])
);
```

## Open Questions

1. **String mutability?**
   - Immutable strings (current)
   - Mutable string type?

2. **Interning?**
   - Intern string literals
   - Manual interning API

3. **Small string optimization?**
   - Store short strings inline
   - Avoid heap allocation

4. **Regex library?**
   - Built-in or optional
   - Which engine (RE2, PCRE)?

## Related Features

- [String Interpolation](string-interpolation.md) - Embedded expressions
- [Traits](traits.md) - ToString, FromStr traits
- [Pattern Matching](pattern-matching.md) - String patterns
