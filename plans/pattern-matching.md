# Pattern Matching

`match` expressions with exhaustiveness checking for expressive control flow.

## Proposed Syntax

```whist
match value {
    0 => print("zero"),
    1..10 => print("small"),
    n if n < 0 => print("negative"),
    _ => print("other"),
}
```

## Match Expressions

Match is an expression that returns a value:

```whist
var description = match status {
    Status::Ok => "success",
    Status::Error => "failure",
    Status::Pending => "waiting",
};
```

## Pattern Types

### Literal Patterns
```whist
match x {
    0 => "zero",
    1 => "one",
    42 => "answer",
    _ => "other",
}
```

### Range Patterns
```whist
match char {
    'a'..'z' => "lowercase",
    'A'..'Z' => "uppercase",
    '0'..'9' => "digit",
    _ => "other",
}
```

### Enum Patterns
```whist
match color {
    Color::Red => "#FF0000",
    Color::Green => "#00FF00",
    Color::Blue => "#0000FF",
}
```

### Struct Patterns
```whist
match point {
    Point { x: 0, y: 0 } => "origin",
    Point { x: 0, y } => "on y-axis at {y}",
    Point { x, y: 0 } => "on x-axis at {x}",
    Point { x, y } => "at ({x}, {y})",
}
```

### Tuple Patterns
```whist
match pair {
    (0, 0) => "origin",
    (0, y) => "y = {y}",
    (x, 0) => "x = {x}",
    (x, y) => "({x}, {y})",
}
```

### Variant Patterns (for Result/Option)
```whist
match result {
    Ok(value) => print("Got: {value}"),
    Err(e) => print("Error: {e}"),
}

match maybe_value {
    Some(x) => use(x),
    None => default(),
}
```

### Or Patterns
```whist
match day {
    Day::Saturday | Day::Sunday => "weekend",
    _ => "weekday",
}
```

### Guard Clauses
```whist
match number {
    n if n < 0 => "negative",
    n if n > 100 => "large",
    n => "normal: {n}",
}
```

### Binding Patterns
```whist
match point {
    p @ Point { x: 0, y: 0 } => use_origin(p),
    Point { x, y } if x == y => "diagonal",
    _ => "other",
}
```

## Exhaustiveness Checking

Compiler ensures all cases are covered:

```whist
enum Direction { North, South, East, West }

match dir {
    Direction::North => go_up(),
    Direction::South => go_down(),
    // ERROR: non-exhaustive, missing East and West
}

// Fixed:
match dir {
    Direction::North => go_up(),
    Direction::South => go_down(),
    Direction::East => go_right(),
    Direction::West => go_left(),
}

// Or use wildcard:
match dir {
    Direction::North => go_up(),
    _ => stay(),
}
```

## If-Let Pattern

Simplified single-pattern matching:

```whist
if let Some(value) = maybe_value {
    use(value);
}

// With else
if let Ok(data) = parse(input) {
    process(data);
} else {
    handle_error();
}
```

## While-Let Pattern

Loop while pattern matches:

```whist
while let Some(item) = iterator.next() {
    process(item);
}
```

## Let-Else Pattern

Destructure or diverge:

```whist
let Some(config) = load_config() else {
    return Err("no config");
};
// config is bound here
```

## Implementation Considerations

### Decision Trees

Compile patterns to efficient decision trees:

```whist
match point {
    Point { x: 0, y: 0 } => A,
    Point { x: 0, y } => B,
    Point { x, y: 0 } => C,
    Point { x, y } => D,
}
```

Compiles to:
```
if x == 0:
    if y == 0: A
    else: B
else:
    if y == 0: C
    else: D
```

### Generated C Code

```c
// Match on enum
switch (color.tag) {
    case Color_Red: result = "#FF0000"; break;
    case Color_Green: result = "#00FF00"; break;
    case Color_Blue: result = "#0000FF"; break;
}

// Match with destructuring
if (point.x == 0 && point.y == 0) {
    result = "origin";
} else if (point.x == 0) {
    i64 y = point.y;
    result = format("on y-axis at %lld", y);
} // ...
```

### Exhaustiveness Algorithm

Use standard exhaustiveness checking:
1. Build matrix of patterns vs constructors
2. Check if all constructors are covered
3. Report missing patterns if not exhaustive

## Grammar Changes

```bnf
match_expr = "match" expression "{" match_arm* "}"
match_arm = pattern ("if" expression)? "=>" expression ","

pattern = literal_pattern
        | range_pattern
        | identifier_pattern
        | wildcard_pattern
        | tuple_pattern
        | struct_pattern
        | enum_pattern
        | or_pattern
        | binding_pattern

literal_pattern = INTEGER | FLOAT | STRING | CHAR | "true" | "false"
range_pattern = pattern ".." pattern
identifier_pattern = IDENTIFIER
wildcard_pattern = "_"
tuple_pattern = "(" pattern ("," pattern)* ")"
struct_pattern = IDENTIFIER "{" field_pattern ("," field_pattern)* "}"
enum_pattern = IDENTIFIER "::" IDENTIFIER ("(" pattern ")")?
or_pattern = pattern ("|" pattern)+
binding_pattern = IDENTIFIER "@" pattern
```

## Open Questions

1. Should match be an expression or statement?
2. Syntax for inclusive vs exclusive ranges?
3. Allow patterns in regular `let` statements?
4. How to handle overlapping patterns (warning or error)?
5. Support for array/slice patterns?
6. Nested or patterns `(A | B, C | D)`?

## Examples

```whist
// Parsing command line args
match args {
    ["help"] | ["-h"] | ["--help"] => show_help(),
    ["version"] => show_version(),
    ["run", file] => run_file(file),
    ["run", file, ..rest] => run_with_args(file, rest),
    [] => show_usage(),
    _ => error("unknown command"),
}

// JSON value handling
match json {
    JsonValue::Null => "null",
    JsonValue::Bool(b) => if b { "true" } else { "false" },
    JsonValue::Number(n) => n.to_string(),
    JsonValue::String(s) => "\"{s}\"",
    JsonValue::Array(items) => format_array(items),
    JsonValue::Object(fields) => format_object(fields),
}

// State machine
match state {
    State::Idle => {
        if has_work() {
            State::Working
        } else {
            State::Idle
        }
    },
    State::Working if is_done() => State::Finished,
    State::Working => State::Working,
    State::Finished => State::Idle,
}
```

## Related Features

- [Result/Option types](result-option.md) - Primary use case for pattern matching
- [Traits](traits.md) - Pattern matching on trait objects
- Enums - Enhanced with data-carrying variants
