# Result / Option Types

Structured error handling with explicit success and failure paths.

## Proposed Syntax

```whist
func divide(a: i64, b: i64): Result<i64, string> {
    if b == 0 {
        return Err("division by zero");
    }
    return Ok(a / b);
}

var result = divide(10, 2)?;  // propagate errors
```

## Option Type

Represents a value that may or may not exist:

```whist
enum Option<T> {
    Some(T),
    None,
}

func find(items: Span<i64>, target: i64): Option<i64> {
    foreach i in 0..items.count {
        if items[i] == target {
            return Some(i);
        }
    }
    return None;
}
```

### Option Methods

```whist
// Check and unwrap
if opt.is_some() {
    var value = opt.unwrap();
}

// Unwrap with default
var value = opt.unwrap_or(default);
var value = opt.unwrap_or_else(|| compute_default());

// Transform
var doubled = opt.map(|x| x * 2);

// Chain operations
var result = opt
    .filter(|x| x > 0)
    .map(|x| x * 2);

// Convert to Result
var result = opt.ok_or("value not found");
```

## Result Type

Represents success or failure with error information:

```whist
enum Result<T, E> {
    Ok(T),
    Err(E),
}

func parse_int(s: string): Result<i64, ParseError> {
    // ...
}

func read_file(path: string): Result<string, IoError> {
    // ...
}
```

### Result Methods

```whist
// Check and unwrap
if result.is_ok() {
    var value = result.unwrap();
}

// Unwrap with default
var value = result.unwrap_or(default);

// Transform success value
var doubled = result.map(|x| x * 2);

// Transform error
var mapped = result.map_err(|e| format("Error: {e}"));

// Chain fallible operations
var data = result.and_then(|x| validate(x));

// Convert to Option
var opt = result.ok();
```

## Error Propagation Operator (`?`)

Automatically propagate errors up the call stack:

```whist
func process_file(path: string): Result<Data, Error> {
    var content = read_file(path)?;       // returns early if Err
    var parsed = parse_json(content)?;    // returns early if Err
    var validated = validate(parsed)?;    // returns early if Err
    return Ok(validated);
}
```

The `?` operator desugars to:

```whist
var content = match read_file(path) {
    Ok(v) => v,
    Err(e) => return Err(e),
};
```

### Error Conversion with `?`

When error types differ, auto-convert if `From` trait is implemented:

```whist
func process(): Result<Data, AppError> {
    var file = read_file(path)?;  // IoError -> AppError
    var json = parse(file)?;      // ParseError -> AppError
    return Ok(json);
}

impl From<IoError> for AppError {
    func from(e: IoError): AppError {
        return AppError::Io(e);
    }
}
```

## Try Blocks (Optional)

Scope for `?` operator with local error handling:

```whist
var result = try {
    var a = operation1()?;
    var b = operation2(a)?;
    Ok(b)
};
```

## Implementation Considerations

### Data Layout

Option and Result are just enums with data:

```c
// Option<i64>
typedef struct {
    enum { Option_None, Option_Some } tag;
    union {
        i64 some_value;
    };
} Option_i64;

// Result<i64, string>
typedef struct {
    enum { Result_Ok, Result_Err } tag;
    union {
        i64 ok_value;
        string err_value;
    };
} Result_i64_string;
```

### Null Pointer Optimization

For `Option<&T>`, use null pointer representation:

```c
// Option<&Point> is just a pointer
// None = NULL, Some(p) = p
typedef Point* Option_ptr_Point;
```

### `?` Operator Code Generation

```whist
var x = foo()?;
```

Generates:

```c
Result_i64_string _tmp = foo();
if (_tmp.tag == Result_Err) {
    return (Result_i64_string){ .tag = Result_Err, .err_value = _tmp.err_value };
}
i64 x = _tmp.ok_value;
```

## Grammar Changes

```bnf
// These are just generic enum types, no special grammar needed
// except for the ? operator

postfix_expr = primary_expr postfix_op*
postfix_op = "(" args ")"      // function call
           | "[" expr "]"       // index
           | "." IDENTIFIER     // member access
           | "?"                // error propagation

// Try block (optional feature)
try_expr = "try" block
```

## Standard Definitions

```whist
// In std library
enum Option<T> {
    None,
    Some(T),
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

// Common error type
enum Error {
    Io(IoError),
    Parse(ParseError),
    Custom(string),
}
```

## Integration with Pattern Matching

```whist
match result {
    Ok(value) => print("Success: {value}"),
    Err(e) => print("Failed: {e}"),
}

match option {
    Some(x) if x > 0 => positive(x),
    Some(x) => negative(x),
    None => default(),
}

// If-let for concise handling
if let Some(value) = maybe_value {
    use(value);
}

if let Ok(data) = result {
    process(data);
} else {
    handle_error();
}
```

## Open Questions

1. Should `unwrap()` panic or be compile-time checked?
2. Syntax for `?` in void-returning functions?
3. Allow `?` on Option in Result-returning functions?
4. Stack traces for errors?
5. `anyhow`-style dynamic errors vs typed errors?
6. Should there be a `throws` annotation on functions?

## Examples

```whist
// File processing pipeline
func process_config(): Result<Config, Error> {
    var path = env::var("CONFIG_PATH")?;
    var content = fs::read_to_string(path)?;
    var config = json::parse::<Config>(content)?;
    config.validate()?;
    return Ok(config);
}

// Optional chaining
var name = user
    .profile?
    .display_name
    .unwrap_or("Anonymous");

// Combining multiple Results
func fetch_all(urls: Span<string>): Result<Vec<Response>, Error> {
    var results = Vec::new();
    foreach url in urls {
        var response = fetch(url)?;
        results.push(response);
    }
    return Ok(results);
}

// Error context
var file = fs::read(path)
    .map_err(|e| Error::new("failed to read config: {e}"))?;

// Fallback chain
var config = load_from_env()
    .or_else(|_| load_from_file())
    .or_else(|_| Ok(Config::default()))?;
```

## Related Features

- [Pattern matching](pattern-matching.md) - Required for ergonomic Result/Option handling
- [Traits](traits.md) - `From` trait for error conversion
- Enums with data - Result/Option are generic enums
