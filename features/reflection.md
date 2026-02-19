# Reflection & Source Generation

Compile-time reflection via source generation — inspect types at compile time, generate code at compile time, carry zero (or opt-in minimal) cost at runtime.

## Design Philosophy

Whist compiles to C. Full runtime reflection (like Java/C#) would require a type metadata runtime, garbage-collected strings for type names, and dynamic dispatch infrastructure that conflicts with Whist's zero-overhead goals. Instead, reflection in Whist is **compile-time source generation**: the compiler uses its existing AST, type system, and symbol tables to generate additional Whist (or C) code during compilation.

This follows the principle: **if the compiler knows it, the programmer shouldn't have to write it by hand.**

## Phased Approach

### Phase 1: Built-in Derive Macros

Attribute-triggered code generation for a fixed set of compiler-known traits. The compiler walks struct/enum definitions and emits trait implementations automatically.

### Phase 2: Type Descriptor Tables (Opt-in)

Compiler-emitted static metadata structs for types annotated with `@[reflect]`, enabling runtime type introspection where needed (serialization, debugging, ORMs).

### Phase 3: Comptime (wc only)

User-extensible compile-time code generation via [comptime](comptime.md) — an AST interpreter in the self-hosted compiler. Comptime subsumes the need for a separate source generator plugin system.

---

## Phase 1: Built-in Derive Macros

### Syntax

```whist
@[derive(Debug, Clone, Eq, Hash)]
struct Point { x: i64, y: i64 }

@[derive(Debug)]
enum Color { Red, Green, Blue }
```

Attributes use `@[...]` syntax (avoids ambiguity with array indexing, visually distinct from expressions). Multiple derives can appear in a single attribute or across multiple attributes:

```whist
@[derive(Debug)]
@[derive(Clone, Eq)]
struct Pair<K, V> { first: K, second: V }
```

### Derivable Traits

These require the [Traits](traits.md) feature. Until traits are implemented, derives can generate standalone functions instead.

#### Pre-Traits (standalone functions)

Before traits exist, derives generate free functions:

```whist
@[derive(Debug)]
struct Point { x: i64, y: i64 }

// Compiler generates:
func Point_debug(self: *Point) -> string {
    return "Point { x: " + i64_to_string(self->x) + ", y: " + i64_to_string(self->y) + " }";
}
```

#### Post-Traits (trait implementations)

Once traits land, the same `@[derive(Debug)]` generates a proper `impl`:

```whist
// Compiler generates:
impl Debug for Point {
    func (Point) debug_string() -> string {
        return "Point { x: " + self.x.debug_string() + ", y: " + self.y.debug_string() + " }";
    }
}
```

#### Standard Derives

| Derive | Generates | Requirements |
|---|---|---|
| `Debug` | Human-readable string representation | All fields must be Debug |
| `Clone` | Deep copy | All fields must be Clone |
| `Eq` | Structural equality (`==`, `!=`) | All fields must be Eq |
| `Ord` | Ordering (`<`, `>`, `<=`, `>=`, `cmp`) | All fields must be Ord |
| `Hash` | Hash computation | All fields must be Hash |
| `Default` | Zero/empty value construction | All fields must be Default |
| `Serialize` | Structured data output (JSON, etc.) | All fields must be Serialize |
| `Deserialize` | Structured data parsing | All fields must be Deserialize |

### Derive Generation Rules

For a struct with fields `f1: T1, f2: T2, ..., fn: Tn`:

**Debug** — emit `"StructName { f1: " + debug(f1) + ", f2: " + debug(f2) + ... + " }"`.

**Eq** — emit `self.f1 == other.f1 && self.f2 == other.f2 && ... && self.fn == other.fn`.

**Clone** — emit `StructName { f1: clone(self.f1), f2: clone(self.f2), ... }`.

**Hash** — emit chained hash combining: `hash = hash_combine(hash, hash(self.f1)); hash = hash_combine(hash, hash(self.f2)); ...`.

**Ord** — lexicographic comparison: compare `f1` first, if equal compare `f2`, etc.

**Default** — emit `StructName { f1: default(), f2: default(), ... }`.

For enums:

**Debug** — emit a match/switch on the tag returning the variant name as a string.

**Eq** — compare tags.

**Ord** — compare tag values.

### Implementation in w0

Derive generation hooks into the existing pipeline between the checker and codegen:

```
lexer → parser → checker → [derive expansion] → codegen
```

The derive expansion phase:
1. Walk the checked AST for struct/enum declarations with `@[derive(...)]` attributes
2. For each derive, look up the struct's `Type*` from the checker's symbol table
3. Iterate `type->as.struc.field_names` / `field_types` to generate the implementation
4. Emit the generated code as new AST nodes (or directly as C in codegen)

The simplest w0 implementation emits C code directly in codegen rather than synthesizing AST nodes:

```c
// In codegen.c, after emitting a struct definition:
if (has_derive(node, "Debug")) {
    // Emit: const char* Point_debug(Point* self) { ... }
    // Walk type->as.struc.field_names/field_types
    for (int i = 0; i < type->as.struc.field_count; i++) {
        // emit field formatting based on field type
    }
}
```

### Grammar Changes

```bnf
attribute      = "@[" attribute_item ("," attribute_item)* "]"
attribute_item = IDENTIFIER ( "(" attribute_args ")" )?
attribute_args = IDENTIFIER ("," IDENTIFIER)*

struct_decl    = attribute* "struct" IDENTIFIER type_params? "{" field_list "}"
enum_decl      = attribute* "enum" IDENTIFIER "{" enum_values "}"
func_decl      = attribute* "func" ...
```

### AST Changes

Add an attribute list to struct, enum, and function declaration nodes:

```c
// New: Attribute representation
typedef struct {
    char*  name;        // "derive"
    char** args;        // ["Debug", "Clone", "Eq"]
    int    arg_count;
} Attribute;

typedef struct {
    Attribute* attrs;
    int        count;
} AttributeList;

// Modified struct_decl node adds:
struct {
    AttributeList attributes;  // NEW
    int      is_public;
    char*    name;
    int      name_length;
    NodeList fields;
    // ... existing fields ...
} struct_decl;
```

---

## Phase 2: Type Descriptor Tables

Opt-in static type metadata for types that need runtime introspection.

### Syntax

```whist
@[reflect]
struct User {
    name: string,
    age: i64,
    active: bool,
}

// Access type info at runtime
func print_fields(user: *User) -> void {
    var info = @type_info(User);
    var i = 0;
    while i < info.field_count {
        std.print(info.fields[i].name);
        std.print(": ");
        std.print(info.fields[i].type_name);
        std.print("\n");
        i += 1;
    }
}
```

### Generated Metadata

For each `@[reflect]`-annotated type, the compiler emits static descriptor tables in the generated C:

```c
// Generated C code for @[reflect] struct User
static FieldInfo User_fields[] = {
    { "name",   "string", offsetof(User, name),   FIELD_TYPE_STRING },
    { "age",    "i64",    offsetof(User, age),     FIELD_TYPE_I64    },
    { "active", "bool",   offsetof(User, active),  FIELD_TYPE_BOOL   },
};

static TypeInfo User_type_info = {
    .name        = "User",
    .kind        = TYPE_KIND_STRUCT,
    .size        = sizeof(User),
    .field_count = 3,
    .fields      = User_fields,
};
```

### TypeInfo API

```whist
// Built-in types provided by the compiler
struct FieldInfo {
    name: string,
    type_name: string,
    offset: u64,
    type_kind: u8,
}

struct EnumValueInfo {
    name: string,
    value: i64,
}

struct TypeInfo {
    name: string,
    kind: u8,            // struct, enum, builtin, array, ...
    size: u64,
    field_count: i32,
    fields: Span<FieldInfo>,
    enum_value_count: i32,
    enum_values: Span<EnumValueInfo>,
}
```

### Compile-Time Type Info Builtin

`@type_info(T)` is a compiler builtin that resolves to the static `TypeInfo` for type `T`. It is a compile-time expression — the type must be known at compile time and must have `@[reflect]`:

```whist
var info = @type_info(User);       // OK: User has @[reflect]
var info2 = @type_info(i64);       // OK: builtins always have type info
var info3 = @type_info(SomeType);  // ERROR: SomeType not annotated with @[reflect]
```

### Field Access by Name

For `@[reflect]` types, the compiler can generate typed getter/setter functions:

```whist
@[reflect]
struct Config {
    host: string,
    port: i64,
    debug: bool,
}

// Compiler generates:
func Config_get_field(self: *Config, name: string, out: *void) -> bool { ... }
func Config_set_field(self: *Config, name: string, value: *void) -> bool { ... }
```

This enables generic serialization, ORM mapping, and config loading without manual boilerplate.

---

## Phase 3: Comptime (wc only)

Phase 3 is not implemented in w0. Instead, it is provided by the self-hosted compiler (wc) via [comptime](comptime.md) — an AST interpreter that can introspect types and generate code at compile time.

Comptime replaces the need for a separate source generator plugin system. Users write comptime functions in ordinary Whist that use compiler builtins (`@type_info`, `@fields`) to introspect types and return generated source:

```whist
comptime func generate_to_json(comptime T: type) -> string {
    var info = @type_info(T);
    var out = "func " + info.name + "_to_json(self: *" + info.name + ") -> string {\n";
    out += "    var result = \"{\";\n";
    foreach i in 0..info.field_count {
        if i > 0 { out += "    result += \", \";\n"; }
        out += "    result += \"\\\"" + info.fields[i].name + "\\\": \";\n";
        out += "    result += to_string(self->" + info.fields[i].name + ");\n";
    }
    out += "    result += \"}\";\n";
    out += "    return result;\n";
    out += "}\n";
    return out;
}
```

See the [Comptime design document](comptime.md) for full details on the AST interpreter, builtins, and execution model.

---

## Implementation Considerations

### Phase 1 Implementation Path (w0)

Phase 1 is implementable in the current w0 bootstrap compiler with minimal changes:

1. **Lexer**: Add `@` token and `[` `]` handling for attribute syntax
2. **Parser**: Parse attributes before struct/enum/func declarations, store in AST
3. **Checker**: Validate derive names, verify field types satisfy derive requirements (e.g., all fields are Eq for `@[derive(Eq)]`)
4. **Codegen**: After emitting a struct/enum, check for derives and emit corresponding C functions

The attribute `@[...]` syntax is chosen to:
- Avoid conflict with existing array `[n]T` syntax (the `@` prefix disambiguates)
- Be visually distinct from expressions
- Support future extension to functions, fields, parameters

### Interaction with Generics

Derives on generic structs generate code per monomorphized instance:

```whist
@[derive(Debug, Eq)]
struct Pair<K, V> { first: K, second: V }

var p = Pair<i64, string> { first: 1, second: "hello" };
// Uses generated Pair_i64_string_debug() and Pair_i64_string_eq()
```

The derive expansion runs after generic monomorphization, so it sees concrete types and can generate correct field-level operations.

### Interaction with Enums

Current enums are tag-only. Derives handle this directly:

```whist
@[derive(Debug, Eq)]
enum Direction { North, South, East, West }

// Generates:
// const char* Direction_debug(Direction self) {
//     switch (self) {
//         case Direction_North: return "North";
//         case Direction_South: return "South";
//         ...
//     }
// }
// bool Direction_eq(Direction a, Direction b) { return a == b; }
```

When enums gain data variants (union types), derives will need to recursively handle variant payloads.

### Interaction with Traits

When traits are implemented, derives transition from generating standalone functions to generating `impl` blocks. The attribute syntax and derive names remain identical — only the generated output changes. This is a backward-compatible evolution.

### Error Messages

Clear errors when derives fail:

```
error: cannot derive Eq for struct Matrix
  --> matrix.w:3:1
   |
3  | @[derive(Eq)]
   |          ^^ Eq requires all fields to implement Eq
   |
5  |     data: *f64,
   |           ^^^^ pointer type *f64 does not implement Eq
```

### Generated Code Visibility

For debugging, `w0 --ast` or a new `w0 --derived` flag can show the generated code before it hits codegen. This makes derive behavior transparent and debuggable.

---

## Open Questions

1. **Attribute syntax**: `@[derive(...)]` vs `#[derive(...)]` vs `@derive(...)` — `@[...]` is proposed here, but alternatives exist
2. **Field-level attributes**: Should individual fields support attributes? e.g., `@[json(rename = "user_name")] name: string` — useful for serialization but adds parser complexity
3. **Derive ordering**: Does the order of derives matter? (Probably not, but some languages enforce it)
4. **Custom derive names**: In Phase 1, should we allow `@[derive(MyCustom)]` that looks for a function `derive_MyCustom`? Or strictly limit to built-in derives until comptime is available in wc?
5. **Conditional derives**: Should derives be conditional on feature flags or target platform?
6. **Phase 2 timing**: Should `@[reflect]` imply certain derives automatically (e.g., always derive Debug for reflected types)?

## Examples

### Serialization (Phase 1)

```whist
@[derive(Serialize)]
struct Config {
    host: string,
    port: i64,
    max_connections: i32,
}

func save_config(config: *Config, path: string) -> void {
    var json = Config_serialize(config);
    std.write_file(path, json);
}
```

### Debug Printing (Phase 1)

```whist
@[derive(Debug)]
struct Point { x: i64, y: i64 }

@[derive(Debug)]
struct Line { start: Point, end: Point }

func main() -> i32 {
    var line = Line {
        start: Point { x: 0, y: 0 },
        end: Point { x: 10, y: 20 },
    };
    std.print(Line_debug(&line));
    // Output: Line { start: Point { x: 0, y: 0 }, end: Point { x: 10, y: 20 } }
    return 0;
}
```

### Runtime Introspection (Phase 2)

```whist
@[reflect]
struct User {
    name: string,
    age: i64,
    email: string,
}

func print_type_info() -> void {
    var info = @type_info(User);
    std.print("Type: " + info.name + "\n");
    std.print("Size: " + u64_to_string(info.size) + " bytes\n");
    std.print("Fields:\n");
    foreach i in 0..info.field_count {
        std.print("  " + info.fields[i].name + ": " + info.fields[i].type_name + "\n");
    }
}
// Output:
// Type: User
// Size: 24 bytes
// Fields:
//   name: string
//   age: i64
//   email: string
```

### Comptime Source Generation (Phase 3, wc only)

```whist
@[json]
struct User {
    id: i64,
    name: string,
    email: string,
}

// comptime generates User_to_json(), User_from_json(), etc.

func main() -> i32 {
    var user = User { id: 1, name: "Alice", email: "alice@example.com" };
    std.print(User_to_json(&user));
    return 0;
}
```

See [Comptime](comptime.md) for more examples.

## Related Features

- [Comptime](comptime.md) — User-extensible compile-time code generation (Phase 3, wc only)
- [Traits](traits.md) — Derives generate trait implementations; standard traits define the derivable interfaces
- [String Interpolation](string-interpolation.md) — Debug derive benefits from string interpolation
- [Pattern Matching](pattern-matching.md) — Enum derives may need pattern matching for data variants
- [Union Types](union-types.md) — Derives on unions need to handle variant payloads
- [Result/Option](result-option.md) — Deserialize derives return `Result<T, Error>`
