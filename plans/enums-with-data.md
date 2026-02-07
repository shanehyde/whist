# Enums with Data (Tagged Unions)

Support for enums with associated data, enabling types like `Option<T>` and `Result<T, E>`.

## Current State

Phase 1 (non-generic enums with data) is complete. Simple enums and data enums both work end-to-end:

```whist
enum Color { Red, Green, Blue }
var c = Color::Red;

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}
var s = Shape::Circle(3.14);
var tag: i32 = s.tag;
```

Generated C for simple enums: `typedef enum Color { Color_Red, Color_Green, Color_Blue } Color;`

Generated C for data enums: tag enum + tagged union struct (see Phase 1 below).

## Target

```whist
enum Option<T> {
    None,
    Some(T),
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}

enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}
```

## Implementation Phases

### Phase 1: Non-Generic Enums with Data ✅

**Status: Complete**

Added support for enum variants that carry payload data. No generics yet.

```whist
enum Shape {
    Circle(f64),
    Rect(f64, f64),
    None,
}

var s = Shape::Circle(3.14);
var r = Shape::Rect(10.0, 20.0);
var n = Shape::None;
var tag: i32 = s.tag;  // .tag member access
```

**C codegen output:**

```c
typedef enum Shape_Tag {
    Shape_Circle,
    Shape_Rect,
    Shape_None
} Shape_Tag;

typedef struct Shape {
    Shape_Tag tag;
    union {
        struct { double f0; } Circle;
        struct { double f0; double f1; } Rect;
    };
} Shape;

// Construction:
Shape s = (Shape){ .tag = Shape_Circle, .Circle = { .f0 = 3.14 } };
Shape r = (Shape){ .tag = Shape_Rect, .Rect = { .f0 = 10.0, .f1 = 20.0 } };
Shape n = (Shape){ .tag = Shape_None };
```

**Changes made:**

| File | Change |
|------|--------|
| `ast.h` | Added `NODE_ENUM_VARIANT` node type; `enum_variant` struct with name + payload type nodes; extended `enum_value` with `args` (NodeList) and `is_data_enum` flag |
| `ast.c` | `node_free` cases for new/changed nodes |
| `types.h` | `Type.as.enm` extended with `has_data`, `variant_types` (Type***), `variant_type_counts` (int*) |
| `types.c` | Updated `type_enum()` init and `type_free()` cleanup |
| `parser.c` | `parse_enum_decl` creates `NODE_ENUM_VARIANT` nodes with optional `(Type, ...)`; `parse_primary_expression` parses constructor args `Shape::Circle(3.14)` |
| `checker.c` | Validates variant types, constructor arg counts/types, sets `is_data_enum`; `.tag` member access returns `i32` |
| `codegen.c` | Enum typedefs moved to early pass; data enums emit tag enum + tagged union struct; values emit compound literals; `enum_names` tracking for correct type emission (no `*` for enums); type inference for enum value init |
| `codegen.h` | Added `enum_names` / `enum_name_count` / `enum_name_capacity` to CodeGen |
| `print_ast.c` | `NODE_ENUM_VARIANT` handler; updated `NODE_ENUM_VALUE` to show args |
| `grammar.md` | Updated enum BNF: `<enum-variant> ::= <identifier> [ '(' <type> { ',' <type> } ')' ] [ ',' ]` |

**Breaking change:** Simple enum values now emit qualified names (`Color_Red` instead of `Red`) for consistency with data enum tag naming.

**Tests:** `enum_data.w`, `enum_data_tag.w`, `error_enum_data_args.w`, `error_enum_data_type.w`

**Known limitations:**
- No generic enums (Phase 2)
- No `match` for destructuring (Phase 3)
- No methods on enum types
- Enum types as variant fields would emit `EnumName*` instead of `EnumName` in `emit_type`'s AST-node-based path (not yet tested)

### Phase 2: Generic Enums

Extend Phase 1 with generic type parameters, reusing the existing generic instantiation machinery from structs.

```whist
enum Option<T> {
    None,
    Some(T),
}

enum Result<T, E> {
    Ok(T),
    Err(E),
}
```

**C codegen target (after monomorphization):**

```c
typedef enum Option_i64_Tag { Option_i64_None, Option_i64_Some } Option_i64_Tag;

typedef struct Option_i64 {
    Option_i64_Tag tag;
    union {
        struct { int64_t f0; } Some;
    };
} Option_i64;
```

**Changes required:**

- Add `type_params` / `type_param_count` to `enum_decl` AST node (same as `struct_decl`)
- Generic enum instantiation in checker (parallel to struct instantiation)
- Mangled names for monomorphized enum types

### Phase 3: Match Expressions

Required for ergonomic destructuring of enum data.

```whist
match shape {
    Circle(r) => std.print_f64(r),
    Rect(w, h) => std.print_f64(w * h),
    None => std.print("nothing"),
}
```

**C codegen target:**

```c
switch (shape.tag) {
    case Shape_Circle: {
        double r = shape.Circle.f0;
        // ...
        break;
    }
    case Shape_Rect: {
        double w = shape.Rect.f0;
        double h = shape.Rect.f1;
        // ...
        break;
    }
    case Shape_None: {
        // ...
        break;
    }
}
```

This is a separate feature — see [pattern-matching.md](pattern-matching.md).

## Design Decisions

### Value types, not heap-allocated

Enums with data are stack-allocated structs (value types). Unlike Whist structs which are heap-allocated `Type*` pointers, enums are passed by value. This matches C tagged union semantics and avoids RC overhead for small types like `Option<i64>`.

### Anonymous union with named variant structs

Each variant's payload is a named struct inside an anonymous union. Field names are positional (`f0`, `f1`, ...). This gives clean generated C and straightforward field access.

### Variant construction looks like function calls

`Shape::Circle(3.14)` uses the existing `EnumName::ValueName` syntax extended with arguments. Variants without data keep the current syntax: `Shape::None`.

### Tag enum is a separate typedef

The tag enum (`Shape_Tag`) is a separate C typedef from the struct. This allows `switch` on the tag directly.

## Related Features

- [Result/Option types](result-option.md) — Built on top of generic enums with data
- [Pattern matching](pattern-matching.md) — Required for ergonomic enum destructuring
