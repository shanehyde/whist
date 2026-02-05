# Union Types

Values that can be one of several types, with type-safe access.

## Proposed Syntax

```whist
type JsonValue = null | bool | i64 | f64 | string | JsonArray | JsonObject;

var value: JsonValue = 42;
value = "hello";
value = true;
```

## Two Flavors of Unions

### 1. Tagged Unions (Sum Types / Discriminated Unions)

Each variant is explicitly labeled and can carry data:

```whist
type Result<T, E> = Ok(T) | Err(E);
type Option<T> = Some(T) | None;

type Shape =
    | Circle(radius: f64)
    | Rectangle(width: f64, height: f64)
    | Triangle(a: f64, b: f64, c: f64);

var shape: Shape = Circle(5.0);
```

This is essentially what Whist enums with data would become.

### 2. Untagged Unions (Type Unions)

Ad-hoc combination of existing types:

```whist
type StringOrInt = string | i64;
type Nullable<T> = T | null;

var id: string | i64 = "abc123";
id = 42;  // also valid
```

## Accessing Union Values

### Pattern Matching (Primary Method)

```whist
func area(shape: Shape): f64 {
    return match shape {
        Circle(r) => 3.14159 * r * r,
        Rectangle(w, h) => w * h,
        Triangle(a, b, c) => {
            var s = (a + b + c) / 2.0;
            return sqrt(s * (s-a) * (s-b) * (s-c));
        },
    };
}

func process(value: JsonValue): void {
    match value {
        null => print("null"),
        b: bool => print("bool: {b}"),
        n: i64 => print("int: {n}"),
        n: f64 => print("float: {n}"),
        s: string => print("string: {s}"),
        arr: JsonArray => print("array of {arr.len}"),
        obj: JsonObject => print("object"),
    }
}
```

### Type Narrowing (Flow Typing)

```whist
func handle(value: string | i64): void {
    if value is string {
        // value is narrowed to string here
        print(value.to_upper());
    } else {
        // value is narrowed to i64 here
        print(value * 2);
    }
}

func maybe_process(opt: Option<Data>): void {
    if opt is Some(data) {
        process(data);
    }
}
```

### Type Assertions (Unsafe)

```whist
var value: string | i64 = get_value();

// Unsafe cast - panics if wrong type
var s = value as! string;

// Safe cast - returns Option
var maybe_s = value as? string;  // Option<string>
```

## Type Compatibility

### Widening

A value of type `T` is assignable to `T | U`:

```whist
var x: i64 = 42;
var y: i64 | string = x;  // OK: i64 widens to i64 | string
```

### Narrowing

Requires pattern matching or type check:

```whist
var x: i64 | string = get_value();
var y: i64 = x;  // ERROR: might be string

if x is i64 {
    var y: i64 = x;  // OK: narrowed by type check
}
```

### Union Simplification

```whist
type A = i64 | i64;           // simplifies to i64
type B = i64 | string | i64;  // simplifies to i64 | string
type C = (i64 | string) | bool;  // simplifies to i64 | string | bool
```

## Nullable Types (Special Case)

The `?T` syntax as sugar for `T | null`:

```whist
var name: ?string = null;      // same as: string | null
var count: ?i64 = 42;

// Null checking narrows the type
if name != null {
    print(name.length);  // name is string here
}

// Null coalescing
var display = name ?? "Anonymous";

// Optional chaining
var len = name?.length ?? 0;
```

## Tagged Union Syntax Options

### Option A: Enum-style (Current Whist Enums Extended)

```whist
enum Result<T, E> {
    Ok(T),
    Err(E),
}

enum Shape {
    Circle { radius: f64 },
    Rectangle { width: f64, height: f64 },
    Point,  // no data
}
```

### Option B: Type Alias with Variants

```whist
type Result<T, E> = Ok(T) | Err(E);
type Shape = Circle(f64) | Rectangle(f64, f64) | Point;
```

### Option C: Separate Variant Declarations

```whist
variant Ok<T>(value: T);
variant Err<E>(error: E);
type Result<T, E> = Ok<T> | Err<E>;
```

## Implementation Considerations

### Tagged Union Layout

```c
// type Shape = Circle(f64) | Rectangle(f64, f64) | Point

typedef struct {
    enum { Shape_Circle, Shape_Rectangle, Shape_Point } tag;
    union {
        struct { double radius; } circle;
        struct { double width; double height; } rectangle;
        // Point has no data
    } data;
} Shape;
```

### Untagged Union Layout

For untagged unions, need runtime type information:

```c
// type JsonValue = null | bool | i64 | f64 | string

typedef struct {
    enum {
        JsonValue_null,
        JsonValue_bool,
        JsonValue_i64,
        JsonValue_f64,
        JsonValue_string
    } tag;
    union {
        bool bool_val;
        int64_t i64_val;
        double f64_val;
        char* string_val;
    } data;
} JsonValue;
```

Note: Untagged unions still need a tag at runtime to know which type is stored. The "untagged" refers to the source syntax, not the implementation.

### Size Optimization

Union size is max of all variants plus tag:

```whist
type Small = i8 | i16;           // 1 byte tag + 2 bytes data = 3 bytes (+ padding)
type Large = i64 | [1000]i8;     // 1 byte tag + 1000 bytes = 1001 bytes
```

Consider pointer boxing for large variants:

```c
// Auto-box variants larger than threshold
typedef struct {
    uint8_t tag;
    union {
        int64_t i64_val;         // inline: small
        LargeStruct* large_ptr;  // boxed: large
    };
} OptimizedUnion;
```

### Null Pointer Optimization

For `Option<&T>` or `?&T`, use null pointer directly:

```c
// ?&Point is just Point*
// None = NULL, Some(p) = p
typedef Point* Option_ptr_Point;
```

## Type Checking

### Exhaustiveness

Pattern matching must cover all variants:

```whist
func describe(shape: Shape): string {
    return match shape {
        Circle(r) => "circle",
        Rectangle(w, h) => "rectangle",
        // ERROR: non-exhaustive, missing Point
    };
}
```

### Type Inference

Infer union type from usage:

```whist
func get_id(use_string: bool): string | i64 {
    if use_string {
        return "abc";   // inferred as part of return union
    }
    return 123;         // inferred as part of return union
}
```

### Recursive Unions

Unions can reference themselves (for trees, lists, etc.):

```whist
type Expr =
    | Literal(i64)
    | Add(Expr, Expr)
    | Mul(Expr, Expr);

// Must use indirection for recursive types
type List<T> = Nil | Cons(T, Box<List<T>>);
```

## Interaction with Other Features

### With Generics

```whist
type Result<T, E> = Ok(T) | Err(E);
type Option<T> = Some(T) | None;

func map<T, U>(opt: Option<T>, f: func(T): U): Option<U> {
    return match opt {
        Some(x) => Some(f(x)),
        None => None,
    };
}
```

### With Traits

```whist
trait Display {
    func display(): string;
}

// Implement trait for union
impl Display for Shape {
    func (Shape) display(): string {
        return match self {
            Circle(r) => "Circle({r})",
            Rectangle(w, h) => "Rectangle({w}x{h})",
            Point => "Point",
        };
    }
}
```

### With Pattern Matching

See [pattern-matching.md](pattern-matching.md) for full integration.

```whist
match value {
    Ok(x) if x > 0 => positive(x),
    Ok(x) => non_positive(x),
    Err(e) => handle_error(e),
}
```

## Open Questions

1. **Tagged vs untagged unions?**
   - Support both? Only tagged?
   - Different syntax for each?

2. **Syntax for tagged unions?**
   - Extend existing `enum` keyword?
   - New `union` or `type` keyword?
   - Inline variant syntax `Circle(f64) | Rectangle(f64, f64)`?

3. **Type narrowing scope?**
   - Only in `if` branches?
   - Across statements after check?
   - How to handle reassignment?

4. **Null handling?**
   - `?T` as sugar for `T | null`?
   - Dedicated `Option<T>` type?
   - Both?

5. **Recursive unions?**
   - Require explicit `Box<T>` for indirection?
   - Auto-box recursive variants?

6. **Structural vs nominal?**
   - Is `Ok(i64) | Err(string)` the same as `Result<i64, string>`?
   - Nominal (by name) is simpler and safer

7. **Empty union?**
   - Type `Never` with no variants (uninhabited type)?
   - Useful for `Result<T, Never>` meaning "can't fail"

## Examples

```whist
// JSON parser result
type JsonValue =
    | JsonNull
    | JsonBool(bool)
    | JsonNumber(f64)
    | JsonString(string)
    | JsonArray(Vec<JsonValue>)
    | JsonObject(HashMap<string, JsonValue>);

func stringify(value: JsonValue): string {
    return match value {
        JsonNull => "null",
        JsonBool(b) => if b { "true" } else { "false" },
        JsonNumber(n) => n.to_string(),
        JsonString(s) => "\"{s}\"",
        JsonArray(items) => {
            var parts = items.map(stringify);
            return "[" + parts.join(", ") + "]";
        },
        JsonObject(fields) => {
            var parts = fields.map(|(k, v)| "\"{k}\": {stringify(v)}");
            return "{" + parts.join(", ") + "}";
        },
    };
}

// Expression evaluator
type Expr =
    | Lit(i64)
    | Var(string)
    | Add(Box<Expr>, Box<Expr>)
    | Mul(Box<Expr>, Box<Expr>)
    | If(Box<Expr>, Box<Expr>, Box<Expr>);

func eval(expr: Expr, env: HashMap<string, i64>): i64 {
    return match expr {
        Lit(n) => n,
        Var(name) => env.get(name).unwrap(),
        Add(a, b) => eval(*a, env) + eval(*b, env),
        Mul(a, b) => eval(*a, env) * eval(*b, env),
        If(cond, then, else_) => {
            if eval(*cond, env) != 0 {
                eval(*then, env)
            } else {
                eval(*else_, env)
            }
        },
    };
}

// Error handling
type ParseError = UnexpectedToken(Token) | UnexpectedEof | InvalidSyntax(string);
type IoError = NotFound(string) | PermissionDenied | Other(string);
type AppError = Parse(ParseError) | Io(IoError) | Config(string);

func run(): Result<void, AppError> {
    var config = load_config().map_err(|e| AppError::Config(e))?;
    var source = read_file(config.input).map_err(|e| AppError::Io(e))?;
    var ast = parse(source).map_err(|e| AppError::Parse(e))?;
    return Ok(());
}
```

## Related Features

- [Pattern Matching](pattern-matching.md) - Primary way to deconstruct unions
- [Result/Option](result-option.md) - Built on union types
- [Generics](../PLANS.md) - Parameterized union types
- [Traits](traits.md) - Implementing behavior for unions
