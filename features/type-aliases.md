# Type Aliases

**Status: Implemented** (PR #55, `feature/type_aliases` branch)

Named aliases for complex or semantic types. Aliases are purely compile-time — they resolve to the underlying type during type checking and produce no C code.

## Proposed Syntax

```whist
type UserId = i64;
type Callback = func(i32, i32) -> i32;
type StringMap<V> = Map<string, V>;
```

## Use Cases

### 1. Semantic Naming

Give meaning to primitive types:

```whist
type UserId = i64;
type PostId = i64;
type Timestamp = i64;
type Email = string;
type Url = string;

func get_user(id: UserId) -> User { ... }
func get_post(id: PostId) -> Post { ... }
```

### 2. Simplifying Complex Types

Shorten verbose type expressions:

```whist
type JsonObject = HashMap<string, JsonValue>;
type Handler = func(Request) -> Response;
type Middleware = func(Handler) -> Handler;
type Matrix = [[f64; 4]; 4];
type Callback<T> = func(Result<T, Error>) -> void;
```

### 3. Generic Type Partial Application

Fix some type parameters:

```whist
type StringMap<V> = HashMap<string, V>;
type IntResult<E> = Result<i64, E>;
type IoResult<T> = Result<T, IoError>;

var users: StringMap<User> = StringMap::new();
```

### 4. Platform Abstraction

Abstract over platform-specific types:

```whist
#[cfg(target = "windows")]
type FileHandle = WindowsHandle;

#[cfg(target = "unix")]
type FileHandle = i32;
```

## Type Alias vs Newtype

### Type Alias (Transparent)

Alias is interchangeable with the original type:

```whist
type UserId = i64;

var id: UserId = 42;
var num: i64 = id;      // OK: UserId is just i64
var id2: UserId = num;  // OK: i64 is just UserId

func double(x: i64) -> i64 { return x * 2; }
double(id);             // OK: UserId accepted as i64
```

### Newtype (Opaque)

Distinct type that wraps the original:

```whist
newtype UserId = i64;

var id: UserId = UserId(42);
var num: i64 = id;           // ERROR: UserId is not i64
var num: i64 = id.0;         // OK: explicit unwrap
var id2: UserId = num;       // ERROR: i64 is not UserId
var id2: UserId = UserId(num); // OK: explicit wrap
```

### When to Use Each

| Use Case | Type Alias | Newtype |
|----------|------------|---------|
| Readability / documentation | ✓ | ✓ |
| Shorten complex types | ✓ | |
| Prevent mixing similar types | | ✓ |
| Add methods to primitives | | ✓ |
| Zero runtime cost | ✓ | ✓ |

## Syntax Options

### Option A: `type` Keyword (Recommended)

```whist
type UserId = i64;
type Pair<A, B> = (A, B);
```

### Option B: `alias` Keyword

```whist
alias UserId = i64;
alias Pair<A, B> = (A, B);
```

### Option C: `typedef` (C-style)

```whist
typedef i64 UserId;
typedef (A, B) Pair<A, B>;
```

### Newtype Syntax Options

```whist
// Option 1: newtype keyword
newtype UserId = i64;

// Option 2: struct wrapper
struct UserId(i64);

// Option 3: distinct keyword
type UserId = distinct i64;
```

## Generic Type Aliases

```whist
// Simple generic alias
type Pair<A, B> = (A, B);

// Partial application
type StringMap<V> = HashMap<string, V>;

// With constraints (if traits exist)
type Sortable<T: Ord> = Vec<T>;

// Higher-kinded (advanced)
type Functor<F, A> = F<A>;  // F is a type constructor
```

## Recursive Type Aliases

```whist
// Direct recursion - may need special handling
type Json = null | bool | f64 | string | Vec<Json> | HashMap<string, Json>;

// Indirect recursion
type Expr = Literal(i64) | BinOp(Box<Expr>, Op, Box<Expr>);
```

## Implementation Considerations

### Compiler Representation

Type aliases are resolved at compile time:

```whist
type UserId = i64;
var id: UserId = 42;
```

Internally becomes:

```whist
var id: i64 = 42;
```

### No Runtime Cost

Type aliases are purely a compile-time construct:
- No additional memory
- No runtime checks
- No generated code difference

### Cycle Detection

Prevent infinite type alias cycles:

```whist
type A = B;  // ERROR: cycle detected
type B = A;

type C = Vec<C>;  // OK: indirection through Vec
```

### Visibility

Type aliases follow normal visibility rules:

```whist
// In module user.w
public type UserId = i64;      // visible outside module
type InternalId = i64;          // private to module
```

## Interaction with Other Features

### With Generics

```whist
type Result<T> = std.Result<T, Error>;

func parse<T>(s: string) -> Result<T> {
    // ...
}
```

### With Traits

```whist
type Handler = func(Request) -> Response;

// Can't impl trait for alias directly (it's just the underlying type)
// Use newtype if you need to add methods:
newtype Handler = func(Request) -> Response;

impl Handler {
    func (Handler) chain(other: Handler) -> Handler { ... }
}
```

### With Pattern Matching

Aliases are transparent in patterns:

```whist
type Point = (i64, i64);

func origin(p: Point) -> bool {
    match p {
        (0, 0) => true,
        _ => false,
    }
}
```

## Grammar Changes

```bnf
type_alias = "type" IDENTIFIER type_params? "=" type ";"

newtype_decl = "newtype" IDENTIFIER type_params? "=" type ";"
             | "struct" IDENTIFIER type_params? "(" type ")" ";"
```

## Open Questions

1. **Keyword choice?** — Resolved: `type` (matches Rust, TypeScript, Haskell)

2. **Support newtypes?** — Deferred: out of scope for initial implementation

3. **Where can aliases appear?** — Resolved: top-level only (in module declarations)

4. **Generic alias bounds?** — Resolved: supported, bounds are checked at the alias definition site via `<T: Trait>` syntax

5. **Associated type aliases?** — Deferred: not yet implemented

6. **Visibility of underlying type?** — Resolved: aliases are fully transparent; `public type` exports the alias

## Examples

```whist
// Domain modeling
type UserId = i64;
type Email = string;
type Password = string;  // TODO: make this a newtype for safety

type User = struct {
    id: UserId,
    email: Email,
    created_at: Timestamp,
};

// API types
type JsonValue = null | bool | f64 | string | JsonArray | JsonObject;
type JsonArray = Vec<JsonValue>;
type JsonObject = HashMap<string, JsonValue>;

// Function types
type Predicate<T> = func(T) -> bool;
type Comparator<T> = func(T, T) -> Ordering;
type Reducer<T, A> = func(A, T) -> A;

func filter<T>(items: Vec<T>, pred: Predicate<T>) -> Vec<T> { ... }
func sort<T>(items: Vec<T>, cmp: Comparator<T>) -> Vec<T> { ... }
func fold<T, A>(items: Vec<T>, init: A, f: Reducer<T, A>) -> A { ... }

// Platform abstraction
type FileDescriptor = i32;
type SocketHandle = i32;
type ProcessId = i32;

// Complex nested types
type RouteHandler = func(Request, Params) -> Response;
type Middleware = func(RouteHandler) -> RouteHandler;
type Router = HashMap<string, RouteHandler>;

// Generic utilities
type Box<T> = struct { value: T };
type Lazy<T> = func() -> T;
type Cache<K, V> = HashMap<K, Lazy<V>>;
```

## Implemented Scope

- **Use case 1**: Semantic naming (`type UserId = i64;`)
- **Use case 2**: Simplifying complex types (`type Pos = Point;`)
- **Use case 3**: Generic partial application (`type IntBox = Box<i64>;`, `type StringPair<V> = Pair<string, V>;`)
- Cycle detection via depth counter (max 16)
- `public`/`private` visibility
- Codegen resolves aliases to underlying types (no alias names in generated C)

### Not Yet Implemented

- Use case 4: Platform abstraction (requires conditional compilation)
- Newtypes
- Local type aliases (inside functions)
- Associated type aliases (inside traits/impls)
- Function type aliases (`type Handler = func(Request) -> Response;` — requires function types)
- Recursive type aliases through indirection

## Related Features

- [Union Types](union-types.md) - Type aliases for union types
- [Traits](traits.md) - Associated types in traits
- Generics - Generic type aliases
