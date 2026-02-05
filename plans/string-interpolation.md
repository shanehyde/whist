# String Interpolation

Embedded expressions in strings for readable string formatting.

## Proposed Syntax

```whist
var name = "world";
print("Hello {name}!");
print("2 + 2 = {2 + 2}");
```

## Basic Interpolation

Embed any expression inside `{}`:

```whist
var user = "Alice";
var age = 30;

// Variable interpolation
var greeting = "Hello, {user}!";

// Expression interpolation
var message = "Next year you'll be {age + 1}";

// Method calls
var upper = "Name: {user.to_upper()}";

// Nested member access
var info = "User {user.name} has {user.posts.count} posts";
```

## Escape Sequences

Use `{{` and `}}` for literal braces:

```whist
print("Use {{braces}} for interpolation");
// Output: Use {braces} for interpolation

print("JSON: {{\"name\": \"{name}\"}}");
// Output: JSON: {"name": "Alice"}
```

## Format Specifiers

Optional formatting after `:`:

```whist
var pi = 3.14159265;
var count = 42;

// Decimal places
print("Pi is approximately {pi:.2}");  // 3.14

// Padding and alignment
print("Count: {count:>5}");   // "   42" (right-align, width 5)
print("Count: {count:<5}");   // "42   " (left-align)
print("Count: {count:^5}");   // " 42  " (center)

// Zero-padding
print("ID: {id:05}");         // "00042"

// Hex/binary/octal
print("Hex: {value:#x}");     // "0x2a"
print("Binary: {value:#b}");  // "0b101010"

// Sign
print("Value: {num:+}");      // "+42" or "-42"
```

## Multi-line Strings

Interpolation works in multi-line strings:

```whist
var html = "
    <div class=\"{class_name}\">
        <h1>{title}</h1>
        <p>{content}</p>
    </div>
";
```

## Raw Strings (No Interpolation)

Use `r"..."` for raw strings without interpolation:

```whist
var regex = r"^\d{3}-\d{4}$";  // {3} and {4} are literal
var path = r"C:\Users\{name}"; // {name} is literal
```

## Debug Formatting

Use `{=expr}` for debug output showing expression and value:

```whist
var x = 42;
print("{=x}");           // "x = 42"
print("{=x * 2}");       // "x * 2 = 84"
print("{=user.name}");   // "user.name = Alice"
```

## Implementation Considerations

### Parsing

The lexer needs to handle interpolated strings specially:

1. Start string literal on `"`
2. Scan for `{` (not `{{`)
3. Parse expression until matching `}`
4. Continue string until next `{` or `"`

### AST Representation

```whist
// "Hello {name}!" becomes:
StringInterp([
    StringPart("Hello "),
    ExprPart(Identifier("name")),
    StringPart("!"),
])
```

### Code Generation

Convert to concatenation or format function:

```whist
// Input
var s = "Hello {name}, you are {age}!";

// Generated (concatenation approach)
var s = "Hello " + name.to_string() + ", you are " + age.to_string() + "!";

// Generated (format function approach)
var s = _format("Hello %s, you are %s!", name.to_string(), age.to_string());
```

### To-String Conversion

Need a standard way to convert values to strings:

```whist
trait ToString {
    func to_string(): string;
}

// Builtin implementations
impl ToString for i64 { ... }
impl ToString for f64 { ... }
impl ToString for bool { ... }
impl ToString for string {
    func (string) to_string(): string { return self; }
}
```

## Grammar Changes

```bnf
string_literal = '"' string_content* '"'
               | 'r"' raw_string_content '"'

string_content = string_chars
               | escape_sequence
               | interpolation

interpolation = "{" expression format_spec? "}"
              | "{=" expression "}"       // debug format

format_spec = ":" format_options

format_options = [[fill]align][sign][#][0][width][.precision][type]
align = "<" | ">" | "^"
sign = "+" | "-" | " "
type = "b" | "o" | "x" | "X" | "e" | "E" | "f" | "?"
```

## Compile-Time Validation

Catch errors at compile time:

```whist
print("Hello {naem}!");  // ERROR: undefined variable 'naem'
print("Value: {x:z}");   // ERROR: invalid format specifier 'z'
print("Count: {x:.2}");  // WARNING: precision on integer has no effect
```

## Open Questions

1. Should interpolation require `$"..."` prefix (like C#)?
2. Format specifier syntax - Rust-like or printf-like?
3. Compile-time format string validation?
4. Custom formatters for user types?
5. Localization support?
6. Performance: eager vs lazy evaluation?

## Examples

```whist
// Logging
func log(level: string, msg: string): void {
    var timestamp = time::now().format("%Y-%m-%d %H:%M:%S");
    print("[{timestamp}] [{level}] {msg}");
}

// SQL (careful - don't use for untrusted input!)
var query = "SELECT * FROM users WHERE id = {user_id}";

// Templates
func render_card(user: User): string {
    return "
        <div class=\"card\">
            <img src=\"{user.avatar_url}\" alt=\"{user.name}\">
            <h2>{user.name}</h2>
            <p>{user.bio}</p>
            <span class=\"followers\">{user.followers.count} followers</span>
        </div>
    ";
}

// Debug output
func debug_point(p: Point): void {
    print("{=p.x} {=p.y}");
    // Output: p.x = 10 p.y = 20
}

// Formatted numbers
func format_currency(amount: f64): string {
    return "${amount:.2}";
}

func format_percentage(ratio: f64): string {
    return "{ratio * 100:.1}%";
}

// Tables
func print_table(items: Span<Item>): void {
    print("{\"Name\":<20} {\"Price\":>10} {\"Qty\":>5}");
    print("{\"\":=<37}");  // separator line
    foreach item in items {
        print("{item.name:<20} {item.price:>10.2} {item.qty:>5}");
    }
}
```

## Related Features

- [Traits](traits.md) - `ToString` trait for custom types
- [Result/Option](result-option.md) - Formatting error messages
- Standard library string functions
