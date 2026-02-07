# Enums with Data (Tagged Unions)

Support for enums with associated data, enabling types like `Option<T>` and `Result<T, E>`.

## Current State

Phase 1 (non-generic enums with data) and Phase 2 (generic enums) are complete. Simple enums, data enums, and generic enums all work end-to-end:

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

enum Option<T> { None, Some(T) }
var x: Option<i64> = Option::Some(42);
var y = Option::Some(3.14);           // inferred as Option<f32>
var n: Option<i64> = Option::None;    // explicit type needed (can't infer from None)

enum Result<T, E> { Ok(T), Err(E) }
var ok: Result<i64, string> = Result::Ok(42);
```

Generated C for simple enums: `typedef enum Color { Color_Red, Color_Green, Color_Blue } Color;`

Generated C for data enums: tag enum + tagged union struct (see Phase 1 below).

Generated C for generic enums (monomorphized):
```c
typedef enum Option_i64_Tag { Option_i64_None, Option_i64_Some } Option_i64_Tag;
typedef struct Option_i64 {
    Option_i64_Tag tag;
    union { struct { int64_t f0; } Some; };
} Option_i64;
```

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
- No `match` for destructuring (Phase 3)
- No methods on enum types

### Phase 2: Generic Enums ✅

**Status: Complete**

Extended Phase 1 with generic type parameters, reusing the existing generic instantiation machinery from structs. Supports type inference from constructor args and explicit annotation for uninferrable cases.

```whist
enum Option<T> { None, Some(T) }
enum Result<T, E> { Ok(T), Err(E) }

var x: Option<i64> = Option::Some(42);   // explicit type
var y = Option::Some(3.14);              // inferred as Option<f32>
var n: Option<i64> = Option::None;       // explicit type needed

var ok: Result<i64, string> = Result::Ok(42);
var err: Result<i64, string> = Result::Err("bad");
```

**C codegen output (monomorphized):**

```c
typedef enum Option_i64_Tag { Option_i64_None, Option_i64_Some } Option_i64_Tag;
typedef struct Option_i64 {
    Option_i64_Tag tag;
    union { struct { int64_t f0; } Some; };
} Option_i64;

// Construction:
Option_i64 x = (Option_i64){.tag = Option_i64_Some, .Some = {.f0 = 42LL}};
Option_i64 n = (Option_i64){.tag = Option_i64_None};
```

**Changes made:**

| File | Change |
|------|--------|
| `ast.h` | Added `type_params`, `type_param_bounds`, `type_param_count` to `enum_decl` |
| `ast.c` | Free type_params in `node_free` for `NODE_ENUM_DECL` |
| `parser.c` | Parse `<T, E>` type params after enum name (same pattern as struct) |
| `checker.h` | Added `enum_target_hint` field to Checker for type inference fallback |
| `checker.c` | Register generic enum defs; `instantiate_generic_enum()` helper; type param inference from constructor args; `enum_target_hint` for uninferrable cases (e.g. `Option::None`); modified `NODE_GENERIC_TYPE` in `resolve_type` to branch on enum vs struct |
| `codegen.c` | `build_mangled_name_from_generic_node()` helper; `resolve_enum_name()` for RC dispatch; `find_generic_enum_decl()` helper; fixed `is_struct_type`/`type_node_has_rc`/`emit_type` for generic enums; emit monomorphized tag enum + tagged union typedefs with `subst_ctx`; emit RC inc/dec helpers for generic enum instances with struct-pointer payloads |
| `grammar.md` | `<enum-decl> ::= 'enum' <identifier> [ '<' <type-param-list> '>' ] '{' { <enum-variant> } '}'` |

**Tests:** `generic_enum_basic.w`, `generic_enum_multi.w`, `generic_enum_infer.w`, `generic_enum_tag.w`, `error_generic_enum_infer.w`, `error_generic_enum_arity.w`

**Known limitations:**
- No `match` for destructuring (Phase 3)
- No methods on generic enum types
- `Option::None` requires explicit type annotation (cannot infer T from no arguments)

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
