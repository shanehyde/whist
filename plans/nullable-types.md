# Nullable Types

Explicit null safety with `?T` syntax and optional chaining.

## Proposed Syntax

```whist
var name: ?string = null;
if name != null {
    print(name);  // safely unwrapped
}
var len = name?.length() ?? 0;  // optional chaining
```

## Core Concept

Nullable types make null explicit in the type system:

```whist
var name: string = "Alice";     // cannot be null
var maybe: ?string = null;       // can be null

name = null;    // ERROR: string cannot be null
maybe = null;   // OK: ?string can be null
maybe = "Bob";  // OK: ?string can hold a value
```

## Relationship to Option/Union Types

`?T` is syntactic sugar for a union with null:

```whist
?string  ≡  string | null  ≡  Option<string>
```

Implementation can use any of these representations:
- Tagged union with None/Some variants
- Pointer types: null pointer for None
- Value types: flag byte + value

## Null Checking and Type Narrowing

### If-Null Check

```whist
var name: ?string = get_name();

if name != null {
    // name is narrowed to string here
    print(name.to_upper());
}

if name == null {
    print("no name");
} else {
    // name is string here
    print(name.length);
}
```

### Guard Clauses

```whist
func process(data: ?Data): void {
    if data == null {
        return;
    }
    // data is Data from here on
    data.process();
}

func require_data(data: ?Data): Result<void, Error> {
    if data == null {
        return Err("data required");
    }
    // data is Data here
    use(data);
    return Ok(());
}
```

### Let-Else (if supported)

```whist
func process(input: ?string): Result<i64, Error> {
    let value = input else {
        return Err("input required");
    };
    // value is string here
    return Ok(parse(value));
}
```

## Optional Chaining (`?.`)

Access members of nullable values safely:

```whist
var user: ?User = get_user();

// Without optional chaining
var name: ?string;
if user != null {
    name = user.name;
} else {
    name = null;
}

// With optional chaining
var name = user?.name;  // ?string

// Chained access
var city = user?.address?.city;  // ?string

// Method calls
var upper = user?.name?.to_upper();  // ?string

// Indexing
var first = user?.posts?[0];  // ?Post
```

### Chaining Semantics

If any part is null, the entire chain evaluates to null:

```whist
var x: ?Outer = get_outer();
var result = x?.middle?.inner?.value;

// Equivalent to:
var result: ?Value;
if x != null {
    var m = x.middle;
    if m != null {
        var i = m.inner;
        if i != null {
            result = i.value;
        } else {
            result = null;
        }
    } else {
        result = null;
    }
} else {
    result = null;
}
```

## Null Coalescing (`??`)

Provide a default value when null:

```whist
var name = user?.name ?? "Anonymous";
var count = get_count() ?? 0;
var config = load_config() ?? Config::default();
```

### Coalescing with Side Effects

Right side is only evaluated if left is null:

```whist
var value = cached_value() ?? expensive_compute();
// expensive_compute only called if cached_value returns null
```

### Chaining Coalescing

```whist
var name = user?.nickname ?? user?.full_name ?? "Unknown";
var config = env_config() ?? file_config() ?? default_config();
```

## Null Coalescing Assignment (`??=`)

Assign only if currently null:

```whist
var cache: ?Data = null;

cache ??= load_data();  // assigns load_data() to cache
cache ??= load_data();  // does nothing, cache already has value
```

## Forced Unwrap (`!`)

Assert that a value is not null (panics if null):

```whist
var name: ?string = get_name();
var definite: string = name!;  // panics if name is null

// Use when you're certain it's not null
func process_valid_user(user: User): void {
    // user.email is required field, never null
    send_email(user.email!);
}
```

### Safety Considerations

Forced unwrap should be used sparingly:
- When null would indicate a bug
- After validation that guarantees non-null
- In tests

Consider pattern matching or `??` for production code.

## Nullable in Function Signatures

### Nullable Parameters

```whist
func greet(name: ?string): void {
    print("Hello, {name ?? \"stranger\"}!");
}

greet("Alice");  // Hello, Alice!
greet(null);     // Hello, stranger!
```

### Nullable Return Types

```whist
func find(items: Span<i64>, target: i64): ?i64 {
    foreach i in 0..items.count {
        if items[i] == target {
            return i;  // implicitly wrapped
        }
    }
    return null;
}

var index = find(numbers, 42);
if index != null {
    print("Found at {index}");
}
```

## Nullable Fields

```whist
struct User {
    id: i64,
    name: string,
    email: ?string,      // optional
    phone: ?string,      // optional
    address: ?Address,   // optional
}

var user = User {
    id: 1,
    name: "Alice",
    email: "alice@example.com",
    phone: null,
    address: null,
};
```

## Implementation Considerations

### Representation

**Pointer types**: Use null pointer directly

```c
// ?&Point in Whist
Point* nullable_point;  // NULL = none, non-NULL = some
```

**Value types**: Need a flag

```c
// ?i64 in Whist
typedef struct {
    bool has_value;
    int64_t value;
} Option_i64;
```

**Optimized representations**:
- `?bool`: Use 0, 1, 2 (false, true, null)
- `?char`: Use invalid Unicode value for null
- `?enum`: Use out-of-range value for null

### Null Pointer Optimization

When `T` has invalid bit patterns, use them for null:

```whist
?&User     // Just a pointer, null = none
?func()    // Just a function pointer
?Box<T>    // Just a pointer

?i64       // Needs wrapper, all bit patterns valid
?bool      // Could use third value
```

### Generated C Code

```whist
var x: ?i64 = get_value();
var y = x ?? 0;
```

Generates:

```c
Option_i64 x = get_value();
int64_t y = x.has_value ? x.value : 0;
```

```whist
var name = user?.profile?.name;
```

Generates:

```c
Option_string name;
if (user.has_value && user.value.profile.has_value) {
    name = (Option_string){ .has_value = true, .value = user.value.profile.value.name };
} else {
    name = (Option_string){ .has_value = false };
}
```

## Interaction with Other Features

### With Pattern Matching

```whist
match maybe_value {
    null => handle_missing(),
    value => use(value),
}

// Or with Option-style naming
match maybe_value {
    Some(v) => use(v),
    None => handle_missing(),
}
```

### With Generics

```whist
func unwrap_or<T>(opt: ?T, default: T): T {
    if opt != null {
        return opt;
    }
    return default;
}

func map<T, U>(opt: ?T, f: func(T): U): ?U {
    if opt != null {
        return f(opt);
    }
    return null;
}
```

### With Traits

```whist
// Could have a trait for "maybe" values
trait Unwrappable<T> {
    func unwrap(): T;
    func unwrap_or(default: T): T;
    func is_some(): bool;
    func is_none(): bool;
}
```

### With Result Types

```whist
// Convert between Option and Result
func ok_or<T, E>(opt: ?T, err: E): Result<T, E> {
    if opt != null {
        return Ok(opt);
    }
    return Err(err);
}

// Get Ok value or null
func ok<T, E>(result: Result<T, E>): ?T {
    match result {
        Ok(v) => v,
        Err(_) => null,
    }
}
```

## Grammar Changes

```bnf
type = nullable_type | base_type
nullable_type = "?" base_type

optional_chain = primary_expr ("?." IDENTIFIER)*
optional_index = primary_expr ("?[" expr "]")*
optional_call = primary_expr ("?(" args ")")*

null_coalesce = expr "??" expr
null_assign = lvalue "??=" expr
force_unwrap = expr "!"
```

## Open Questions

1. **`?T` vs `Option<T>`?**
   - Use `?T` as sugar for `Option<T>`?
   - Or separate concepts?

2. **Null literal type?**
   - Is `null` its own type?
   - Or only valid in `?T` context?

3. **Double nullable?**
   - What is `??T`?
   - Flatten to `?T`?
   - Or distinct type?

4. **Optional chaining return type?**
   - `a?.b` returns `?B` even if `b: B`
   - How to avoid `????T` from deep chains?

5. **Truthiness of nullable?**
   - Can `?T` be used in boolean context?
   - `if (maybe_value)` vs `if (maybe_value != null)`?

6. **Nullable references vs values?**
   - Same syntax for both?
   - Different optimization strategies

7. **Interaction with generics?**
   - Is `?T` valid when `T` is already `?U`?
   - Variance of nullable types?

## Examples

```whist
// User lookup with fallbacks
func get_display_name(user_id: i64): string {
    var user = find_user(user_id);
    return user?.profile?.display_name
        ?? user?.username
        ?? "User #{user_id}";
}

// Config loading with defaults
func load_settings(): Settings {
    return Settings {
        theme: config.get("theme") ?? "light",
        font_size: config.get_i64("font_size") ?? 14,
        sidebar: config.get_bool("sidebar") ?? true,
        custom_css: config.get("custom_css"),  // stays nullable
    };
}

// Safe navigation in data structures
func get_nested(json: JsonValue, path: Span<string>): ?JsonValue {
    var current: ?JsonValue = json;
    foreach key in path {
        current = current?.as_object()?.get(key);
    }
    return current;
}

// Optional method chaining
var result = input
    ?.trim()
    ?.parse_i64()
    ?.abs();

// Early return pattern
func process_order(order_id: i64): Result<Receipt, Error> {
    var order = find_order(order_id) ?? return Err("order not found");
    var user = find_user(order.user_id) ?? return Err("user not found");
    var payment = user.default_payment ?? return Err("no payment method");

    return Ok(charge(payment, order.total));
}

// Lazy initialization
struct Cache {
    data: ?Data,
}

impl Cache {
    func (Cache) get(): Data {
        self.data ??= load_data();
        return self.data!;
    }
}
```

## Related Features

- [Union Types](union-types.md) - `?T` as `T | null`
- [Result/Option](result-option.md) - Relationship with Option type
- [Pattern Matching](pattern-matching.md) - Matching on nullable values
