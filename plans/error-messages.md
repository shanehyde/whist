# Better Error Messages

Improved diagnostics with suggestions, context, and helpful hints.

## Goals

1. **Clarity** - Immediately understand what went wrong
2. **Context** - See relevant code with the error
3. **Suggestions** - Offer fixes when possible
4. **Consistency** - Uniform format across all errors
5. **Accessibility** - Work well in terminals and IDEs

## Current State

Basic error messages:
```
error: type mismatch at line 5
```

## Target State

Rich, helpful diagnostics:
```
error[E0308]: type mismatch
  --> src/main.w:5:12
   |
 5 |     var x: string = 42;
   |            ------   ^^ expected `string`, found `i64`
   |            |
   |            expected due to this type annotation
   |
help: consider converting the integer to a string
   |
 5 |     var x: string = 42.to_string();
   |                       ++++++++++++
```

## Error Format

### Structure

```
<severity>[<code>]: <message>
  --> <file>:<line>:<column>
   |
 n |     <source line>
   |     <underlines and labels>
   |
<notes and suggestions>
```

### Severity Levels

```
error[E0001]: this is a fatal error
warning[W0001]: this is a warning
note: this provides additional context
help: this suggests a fix
```

### Error Codes

Unique codes for each error type:

```
E0001 - E0099: Syntax errors
E0100 - E0199: Type errors
E0200 - E0299: Name resolution errors
E0300 - E0399: Borrow/lifetime errors
E0400 - E0499: Import/module errors

W0001 - W0099: Unused warnings
W0100 - W0199: Style warnings
```

## Error Categories

### Syntax Errors

```
error[E0001]: unexpected token
  --> src/main.w:3:15
   |
 3 |     var x = 1 +
   |               ^ expected expression after `+`
   |
help: add the right-hand side of the expression
   |
 3 |     var x = 1 + 2;
   |                 +
```

```
error[E0010]: unclosed delimiter
  --> src/main.w:5:20
   |
 5 |     func foo(a: i64 {
   |                    ^ expected `)` to close this `(`
   |              - unclosed delimiter
   |
 7 |     }
   |     ^ unexpected `}`
```

```
error[E0015]: unterminated string literal
  --> src/main.w:3:12
   |
 3 |     var s = "hello
   |             ^ string literal not terminated
   |
 4 |     var x = 5;
   |
help: add closing quote
   |
 3 |     var s = "hello";
   |                   +
```

### Type Errors

```
error[E0100]: type mismatch
  --> src/main.w:5:20
   |
 5 |     var count: i32 = "hello";
   |                ^^^   ------- this expression has type `string`
   |                |
   |                expected `i32` due to this
   |
note: `string` cannot be converted to `i32`
```

```
error[E0101]: mismatched types in binary operation
  --> src/main.w:4:15
   |
 4 |     var sum = 1 + "hello";
   |               - ^ ------- `string`
   |               |   |
   |               |   cannot add `i64` and `string`
   |               `i64`
   |
note: the `+` operator is not defined for `i64` and `string`
help: consider converting to the same type
   |
 4 |     var sum = 1.to_string() + "hello";
   |                ++++++++++++
```

```
error[E0110]: missing struct field
  --> src/main.w:8:5
   |
 8 |     var p = Point { x: 10 };
   |             ^^^^^ missing field `y`
   |
note: struct `Point` is defined here
  --> src/types.w:1:1
   |
 1 | struct Point { x: i64, y: i64 }
   | ^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
help: add the missing field
   |
 8 |     var p = Point { x: 10, y: /* value */ };
   |                          ++++++++++++++++
```

### Name Resolution Errors

```
error[E0200]: undefined variable
  --> src/main.w:5:12
   |
 5 |     print(mesage);
   |           ^^^^^^ not found in this scope
   |
help: a local variable with a similar name exists
   |
 5 |     print(message);
   |           ~~~~~~~
```

```
error[E0210]: undefined function
  --> src/main.w:3:5
   |
 3 |     prin("hello");
   |     ^^^^ not found in scope
   |
help: a function with a similar name exists
   |
 3 |     print("hello");
   |     ~~~~~
   |
help: did you mean to import `std`?
   |
 1 + import std;
 2 |
 3 |     std.print("hello");
   |     ++++
```

### Module Errors

```
error[E0400]: unqualified access to module member
  --> src/main.w:5:5
   |
 5 |     print("hello");
   |     ^^^^^ must use qualified access
   |
note: `print` is defined in module `std`
help: use qualified access
   |
 5 |     std.print("hello");
   |     ++++
```

```
error[E0410]: private function
  --> src/main.w:7:5
   |
 7 |     utils.internal_helper();
   |           ^^^^^^^^^^^^^^^ this function is private
   |
note: `internal_helper` is defined here
  --> src/utils.w:10:1
   |
10 | func internal_helper(): void {
   | ^^^^^^^^^^^^^^^^^^^^^^^^^^^^
   |
note: consider making it public if intended for external use
  --> src/utils.w:10:1
   |
10 | public func internal_helper(): void {
   | ++++++
```

### Const/Mutability Errors

```
error[E0300]: cannot assign to immutable variable
  --> src/main.w:4:5
   |
 3 |     const x = 5;
   |     ----- first assignment to `x`
 4 |     x = 10;
   |     ^^^^^^ cannot assign twice to constant
   |
help: consider making this a `var` instead of `const`
   |
 3 |     var x = 5;
   |     ~~~
```

## Suggestions and Quick Fixes

### Automatic Fixes

Compiler can suggest concrete fixes:

```
error[E0100]: type mismatch
  ...
help: try using `to_string()`
   |
 5 |     var s: string = num.to_string();
   |                        ++++++++++++
```

### Machine-Applicable Fixes

Mark fixes that can be auto-applied:

```bash
$ wc --fix src/main.w
Fixed 3 errors:
  - Added missing semicolon at line 5
  - Fixed typo: `mesage` → `message` at line 8
  - Added missing import `std` at line 1
```

### Similar Name Suggestions

Use edit distance to suggest alternatives:

```whist
func suggest_similar(name: string, candidates: Vec<string>): Option<string> {
    var best: Option<(string, i32)> = None;

    foreach candidate in candidates {
        var distance = levenshtein(name, candidate);
        if distance <= 3 {  // threshold
            match best {
                None => best = Some((candidate, distance)),
                Some((_, d)) if distance < d => best = Some((candidate, distance)),
                _ => {},
            }
        }
    }

    best.map(|(name, _)| name)
}
```

## Colored Output

Terminal colors for readability:

```
[red]error[E0100][/red]: type mismatch
  [blue]-->[/blue] src/main.w:5:12
   [blue]|[/blue]
 [blue]5[/blue] [blue]|[/blue]     var x: [yellow]string[/yellow] = [yellow]42[/yellow];
   [blue]|[/blue]            [red]------[/red]   [red]^^[/red] expected `string`, found `i64`
   [blue]|[/blue]            [blue]|[/blue]
   [blue]|[/blue]            expected due to this
```

Disable with `--no-color` or `NO_COLOR` env var.

## Multi-Span Errors

Show related locations:

```
error[E0101]: conflicting implementations
  --> src/a.w:5:1
   |
 5 | impl Foo for Bar { ... }
   | ^^^^^^^^^^^^^^^^ first implementation here
   |
  ::: src/b.w:10:1
   |
10 | impl Foo for Bar { ... }
   | ^^^^^^^^^^^^^^^^ conflicting implementation here
```

## Error Recovery

Report multiple errors in one pass:

```
error[E0001]: unexpected token at line 5
error[E0200]: undefined variable `foo` at line 8
error[E0100]: type mismatch at line 12

Found 3 errors; aborting
```

Limit output to avoid overwhelming:

```bash
$ wc src/main.w
error[E0001]: ...
error[E0002]: ...
error[E0003]: ...
... and 47 more errors

error: aborting due to 50 previous errors
help: try fixing the first few errors and recompiling
```

## Implementation

### Diagnostic Structure

```whist
struct Diagnostic {
    severity: Severity,
    code: string,
    message: string,
    primary_span: Span,
    secondary_spans: Vec<(Span, string)>,  // (span, label)
    notes: Vec<string>,
    suggestions: Vec<Suggestion>,
}

struct Span {
    file: string,
    start_line: i32,
    start_col: i32,
    end_line: i32,
    end_col: i32,
}

struct Suggestion {
    message: string,
    replacement: string,
    span: Span,
    applicability: Applicability,
}

enum Applicability {
    MachineApplicable,    // Safe to auto-apply
    MaybeIncorrect,       // Might not be right
    HasPlaceholders,      // Contains `/* ... */`
    Unspecified,          // Just a hint
}
```

### Rendering

```whist
func render_diagnostic(diag: Diagnostic, source: SourceMap): string {
    var out = StringBuilder::new();

    // Header
    out.append("{diag.severity}[{diag.code}]: {diag.message}\n");

    // Primary span
    out.append("  --> {diag.primary_span.file}:");
    out.append("{diag.primary_span.start_line}:");
    out.append("{diag.primary_span.start_col}\n");

    // Source context
    render_span(out, diag.primary_span, source);

    // Secondary spans
    foreach (span, label) in diag.secondary_spans {
        render_span(out, span, source, label);
    }

    // Notes
    foreach note in diag.notes {
        out.append("note: {note}\n");
    }

    // Suggestions
    foreach suggestion in diag.suggestions {
        out.append("help: {suggestion.message}\n");
        render_suggestion(out, suggestion, source);
    }

    return out.to_string();
}
```

## IDE Integration

### LSP Diagnostics

```json
{
    "uri": "file:///path/to/main.w",
    "diagnostics": [
        {
            "range": { "start": {"line": 4, "character": 11}, "end": {...} },
            "severity": 1,
            "code": "E0100",
            "source": "whist",
            "message": "type mismatch: expected `string`, found `i64`",
            "relatedInformation": [
                {
                    "location": { "uri": "...", "range": {...} },
                    "message": "expected due to this type annotation"
                }
            ]
        }
    ]
}
```

### Code Actions

```json
{
    "title": "Convert to string",
    "kind": "quickfix",
    "diagnostics": [{ "code": "E0100", ... }],
    "edit": {
        "changes": {
            "file:///path/to/main.w": [
                {
                    "range": {...},
                    "newText": "42.to_string()"
                }
            ]
        }
    }
}
```

## Open Questions

1. **Error code scheme?**
   - Numeric (E0001)
   - Alphanumeric (E-TYPE-MISMATCH)
   - Hierarchical (E.type.mismatch)

2. **How many suggestions?**
   - Top suggestion only
   - Up to 3 suggestions
   - All possible fixes

3. **Error documentation?**
   - `--explain E0100` for detailed explanation
   - Online documentation
   - Both

4. **Internationalization?**
   - English only initially
   - Support translations later

5. **JSON output?**
   - For IDE/tooling consumption
   - `--error-format=json`

## Examples

### Before and After

**Before:**
```
Error: cannot find value `user_naem`
```

**After:**
```
error[E0200]: cannot find value `user_naem` in this scope
  --> src/main.w:15:12
   |
15 |     print(user_naem);
   |           ^^^^^^^^^ not found in this scope
   |
help: a local variable with a similar name exists
   |
15 |     print(user_name);
   |           ~~~~~~~~~
```

**Before:**
```
Error: invalid operation
```

**After:**
```
error[E0101]: cannot multiply `string` by `i64`
  --> src/main.w:8:17
   |
 8 |     var result = "hello" * 3;
   |                  ------- ^ - `i64`
   |                  |       |
   |                  |       `*` cannot be applied to `string` and `i64`
   |                  `string`
   |
note: the `*` operator is not defined for `string`
help: if you want to repeat a string, use `repeat()`
   |
 8 |     var result = "hello".repeat(3);
   |                         ~~~~~~~~~~
```

## Related Features

- [LSP Server](lsp-server.md) - IDE error integration
- [REPL](repl.md) - Interactive error display
