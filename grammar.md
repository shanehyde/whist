# Whist Language Grammar

This document describes the grammar of the Whist language in BNF (Backus-Naur Form) notation.

## Notation

- `<non-terminal>` — A non-terminal symbol
- `'terminal'` — A terminal symbol (literal token)
- `|` — Alternation (choice)
- `[ ... ]` — Optional (zero or one)
- `{ ... }` — Repetition (zero or more)
- `( ... )` — Grouping

---

## Program Structure

```bnf
<program> ::= { <import-stmt> | <use-stmt> | <declaration> }

<declaration> ::= [ 'public' | 'private' ] <func-defn>
               | [ 'public' | 'private' ] <struct-decl>
               | [ 'public' | 'private' ] <enum-decl>
               | [ 'public' | 'private' ] <trait-decl>
               | [ 'public' | 'private' ] <type-alias>
               | [ 'public' | 'private' ] <var-decl>
               | <impl-decl>
               | [ 'public' | 'private' ] <extern-module>
               | <test-decl>
```

**Visibility:** Top-level declarations are public by default (module-exported scope). Use `private` for file-local linkage. In generated C code, private declarations are prefixed with `static`. The `main` function is always treated as having external linkage regardless of the `private` modifier.

---

## Declarations

### Import Statement

```bnf
<import-stmt> ::= 'import' ( <identifier> | <string-literal> ) ';'
```

Import statements load declarations from external Whist source files. There are two forms:

- **Module import:** `import std;` resolves the module name from the `lib/` directory (e.g., `lib/std.w`). Symbols from module imports must be accessed with module qualification: `std.print("hello")`. Unqualified access is an error unless brought into scope with `use`.
- **Relative import:** `import "./path/to/file.w";` or `import "../file.w";` resolves the path relative to the importing file's directory. String imports must start with `./` or `../`. Symbols from relative imports are merged into the current module and accessed without qualification.

### Use Statement

```bnf
<use-stmt> ::= 'use' <identifier> '.' ( <identifier> | '{' <identifier> { ',' <identifier> } [ ',' ] '}' ) ';'
```

Use statements selectively bring symbols from an imported module into unqualified scope. The module must be imported with `import` before `use`. After `use std.print;`, `print(...)` can be called without the `std.` prefix. Grouped syntax `use std.{print, abs_i64};` brings multiple symbols at once.

### Function Declaration

```bnf
<func-decl> ::= 'func' [ <receiver> ] <identifier> [ '<' <type-param-list> '>' ] '(' [ <param-list> ] ')' [ ':' <return-type> ]
<func-defn> ::= <func-decl> <block>

<receiver> ::= '(' [ 'const' ] <identifier> [ '<' <type-arg-list> '>' ] ')'

<param-list> ::= <param> { ',' <param> } [ ',' '...' ]
               | '...'

<param> ::= [ 'const' ] <identifier> [ ':' <type> ]

<return-type> ::= [ 'const' ] <type>

<extern-func-decl> ::= <func-decl> [ 'as' <identifier> ] ';'

<extern-module> ::= 'extern' <identifier> '{' { [ 'public' | 'private' ] <extern-func-decl> } '}'
```

When an `extern` block has no explicit visibility modifier at top level, functions in the block default to private unless overridden per function.

**Generic free functions:** Free functions (without a receiver) can have type parameters:
- `func identity<T>(x: T): T` — single type parameter
- `func first<A, B>(a: A, b: B): A` — multiple type parameters
- `func get_label<T: Printable>(x: T): string` — type parameter with trait bound

Type arguments are inferred from call-site argument types (no explicit type arguments at call sites). Each unique instantiation produces a monomorphized C function (e.g., `identity_i64`, `identity_string`).

**Generic methods:** Methods can be defined on generic structs using type arguments in the receiver:
- `func (Box<T>) get(): T` — method on any `Box<T>` instantiation
- `func (Pair<i32, Box<T>>) set(v: Box<T>): void` — partially specialized method

`const` may also qualify return types (for example, `func args_view(): const Vec<string>`).
Current limitation: return-type const is signature-level only; type checking treats `T` and `const T`
as compatible (no deep/shallow immutability distinction yet).

### Struct Declaration

```bnf
<struct-decl> ::= 'struct' <identifier> [ '<' <type-param-list> '>' ] '{' { <field-decl> } '}'

<type-param-list> ::= <type-param> { ',' <type-param> }

<type-param> ::= <identifier> [ ':' <identifier> ]

<field-decl> ::= [ 'const' ] <identifier> ':' <type> [ ',' ]
```

**Generic structs:** Structs can be parameterized by one or more type parameters:
- `struct Box<T> { value: T }` — single type parameter
- `struct Pair<K, V> { key: K, value: V }` — multiple type parameters
- `struct Box<T: HasValue> { item: T }` — type parameter with trait bound

Generic structs are monomorphized at compile time, generating specialized C code for each instantiation (e.g., `Box<i64>` becomes `Box_i64`). When a type parameter has a trait bound, the concrete type argument must implement that trait.

### Enum Declaration

```bnf
<enum-decl>    ::= 'enum' <identifier> [ '<' <type-param-list> '>' ] '{' { <enum-variant> } '}'

<enum-variant> ::= <identifier> [ '(' <type> { ',' <type> } ')' ] [ '=' [ '-' ] <int-literal> ] [ ',' ]
```

### Trait Declaration

```bnf
<trait-decl>   ::= 'trait' <identifier> '{' { <trait-method> } '}'

<trait-method> ::= [ 'const' ] 'func' <identifier> '(' [ <param-list> ] ')' [ ':' <return-type> ] ';'
```

Traits define a set of required method signatures that types can implement. Trait methods are declared without a receiver or body — just the function signature followed by a semicolon. Use `const func` to declare methods that require an immutable receiver. Impl blocks must match the const-ness exactly.

### Impl Declaration

```bnf
<impl-decl>   ::= 'impl' <identifier> 'for' <identifier> [ '<' <type-arg-list> '>' ]
                   '{' { <impl-method> } '}'
               | 'impl' <identifier> [ '<' <type-arg-list> '>' ]
                   '{' { <impl-method> } '}'

<impl-method> ::= [ 'const' ] 'func' <identifier> '(' [ <param-list> ] ')' [ ':' <return-type> ] <block>
```

**Trait impl:** `impl Trait for Type { ... }` provides concrete method implementations for a trait on a specific type. Methods inside `impl` blocks do not specify a receiver — it is inferred from the `for Type` clause. Use `const func` for immutable-receiver methods. For generic target types, specify the type parameters on the impl header (e.g., `impl Drop for Box<T>`). All trait methods must be implemented.

**Inherent impl:** `impl Type { ... }` defines methods directly on a type without a trait. This is used for the `init` constructor pattern:

```whist
impl Point {
    func init(x: i64, y: i64) {
        self.x = x;
        self.y = y;
    }
}
```

The `init` method has special constraints: it must not declare a return type, and each type can have at most one `init` method. When a struct has an `init` method, instances must be created with `new Type(args)` — the struct literal form `new Type { ... }` is disallowed.

### Type Alias

```bnf
<type-alias> ::= 'type' <identifier> [ '<' <type-param-list> '>' ] '=' <type> ';'
```

Type aliases create alternative names for existing types. They are purely compile-time — aliases resolve to the underlying type during type checking and produce no C code.

- **Simple alias:** `type UserId = i64;` — semantic naming for primitive types
- **Struct alias:** `type Pos = Point;` — alternative name for a struct type
- **Generic instantiation alias:** `type IntBox = Box<i64>;` — alias for a concrete generic instantiation
- **Generic partial application:** `type StringPair<V> = Pair<string, V>;` — alias with its own type parameters that fills in some arguments of a generic type

Aliases are fully interchangeable with the underlying type: `var id: UserId = 42;` is equivalent to `var id: i64 = 42;`.

### Variable Declaration

```bnf
<var-decl> ::= ( 'var' | 'const' ) <identifier> [ ':' <type> ] [ '=' <expression> ] ';'
            | ( 'var' | 'const' ) <destruct-pattern> [ ':' <type> ] '=' <expression> ';'
            | ( 'var' | 'const' ) <struct-destruct> '=' <expression> ';'

<destruct-pattern> ::= '(' <destruct-element> ',' <destruct-element> { ',' <destruct-element> } ')'

<destruct-element> ::= <identifier>
                    | <destruct-pattern>

<struct-destruct> ::= '{' <struct-destruct-field> { ',' <struct-destruct-field> } '}'

<struct-destruct-field> ::= <identifier> [ ':' <identifier> ]
```

**Tuple destructuring:** `var (a, b) = expr;` and `const (a, b) = expr;` unpack a tuple into individual bindings. The number of elements must match the tuple's arity. Nested patterns are supported: `var (x, (y, z)) = (1, (2, 3));` unpacks nested tuples.

**Struct destructuring:** `var {field1, field2} = expr;` extracts named fields from a struct into local variables with the same names. The expression must evaluate to a struct type, and each identifier must match a field name on that struct. Partial destructuring is allowed (not all fields need to be listed). The underlying struct is kept alive as an RC-tracked temporary for the enclosing scope. Fields can be renamed with `var {field: local_name} = expr;` to bind a field to a different local variable name.

### Test Declaration

```bnf
<test-decl> ::= 'test' <string-literal> <block>
```

Test declarations define inline unit tests. Each test has a name (string literal) and a body block. Test blocks are top-level declarations that cannot have visibility modifiers (`public test` or `private test` is an error).

In normal compilation, test blocks are completely ignored (zero overhead). When invoked with `w0 test`, test blocks are compiled and executed with pass/fail reporting.

**`assert` builtin:** Inside test blocks (or anywhere), `assert(expr)` validates that `expr` is `true`. The argument must be a `bool` expression. On failure, `assert` prints the stringified expression and source location, then marks the test as failed. If a user-defined `assert` function exists in scope, it takes precedence over the builtin.

```whist
test "arithmetic" {
    assert(1 + 1 == 2);
    assert(add(2, 3) == 5);
}
```

---

## Types

```bnf
<type> ::= <identifier>
        | <identifier> '<' <type-arg-list> '>'
        | 'func' '(' [ <type-list> ] ')' [ ':' <type> ]
        | '[' [ <expression> ] ']' <type>
        | '(' <type> ',' <type> { ',' <type> } ')'

<type-arg-list> ::= <type> { ',' <type> }
<type-list> ::= <type> { ',' <type> }
```

**Built-in types** are resolved as identifiers: `void`, `bool`, `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `char`, `string`, `voidptr`. `voidptr` maps to `void*` in C and is used for opaque pointer handles. It supports `null` assignment and equality comparison (`==`/`!=`) with `null`, but no arithmetic.

**Pointer type syntax:** Explicit pointer type syntax (`*T`) is not supported in the bootstrap compiler.

**Generic types:** `Box<i64>` or `Pair<i32, string>` instantiate a generic struct with concrete type arguments. Nested generics are supported: `Box<Pair<i32, i64>>`.

**Tuple types:** `(T1, T2)` or `(T1, T2, T3, ...)` represent fixed-size heterogeneous collections. Tuples must have at least two elements.

**Span types:** `Span<T>` is a builtin generic type representing an immutable view into contiguous memory (array, span, or vec). Spans have:
- `span.count` — number of elements (read-only)
- `span[i]` — bounds-checked element access (panics if out of bounds)
- `span.data` — private (compile error if accessed directly)

Create spans using slice syntax: `var s: Span<i64> = arr[:];`

**Vec types:** `Vec<T>` is a builtin generic type representing a dynamically-growable array. Vecs are RC-managed (created with `new`) and have:
- `vec.count` — number of elements (read-only, `i64`)
- `vec.capacity` — current capacity (read-only, `i64`)
- `vec[i]` — bounds-checked element access (panics if out of bounds)
- `vec[i] = x` — bounds-checked element write
- `vec.push(x)` — append an element
- `vec.insert(i, x)` — insert an element at index `i`
- `vec.remove(i)` — remove and return the element at index `i`
- `vec.swap_remove(i)` — remove index `i` by swapping with the last element
- `vec.pop()` — remove and return the last element as `Option<T>` (`None` if empty)
- `vec.clear()` — remove all elements (decrements RC for struct elements)
- `vec.reserve(n)` — reserve additional capacity
- `vec.shrink_to_fit()` — shrink capacity to current length
- `vec.contains(x)` — membership test (when `T` supports equality)
- `vec.first()` / `vec.last()` — return `Option<T>`
- `vec.sort()` — in-place sort (for orderable primitive `T`)
- `vec.data` — private (compile error if accessed directly)
- Slice syntax produces a `Span<T>`: `vec[:]`, `vec[1:3]`, `vec[2:]`

Create vecs: `var v = new Vec<i64>{};` or `var v = new Vec<i64>{1, 2, 3};`

---

## Statements

```bnf
<statement> ::= <var-decl>
             | <if-stmt>
             | <while-stmt>
             | <for-stmt>
             | <foreach-stmt>
             | <match-stmt>
             | <return-stmt>
             | <break-stmt>
             | <continue-stmt>
             | <defer-stmt>
             | <block>
             | <expr-stmt>

<block> ::= '{' { <statement> } '}'

<if-stmt> ::= 'if' '(' <expression> ')' <block> [ 'else' ( <if-stmt> | <block> ) ]

<while-stmt> ::= 'while' '(' <expression> ')' <block>

<for-stmt> ::= 'for' '(' <for-init> [ <expression> ] ';' [ <expression> ] ')' <block>

<for-init> ::= <var-decl>
            | <expression> ';'
            | ';'

<foreach-stmt> ::= 'foreach' '(' 'const' <identifier> 'in' <expression> '..' <expression> [ 'by' <expression> ] ')' <block>
                 | 'foreach' '(' 'const' <identifier> 'in' <expression> ')' <block>

The first form iterates over a range. The range `start..end` is **end-exclusive**: iterates from `start` up to but not including `end`. For example, `0..5` iterates `0, 1, 2, 3, 4`.

The second form iterates over a collection. Currently supported: `Vec<T>`, `Span<T>`, `string`.

<return-stmt> ::= 'return' [ <expression> ] ';'

<break-stmt> ::= 'break' ';'

<continue-stmt> ::= 'continue' ';'

<defer-stmt> ::= 'defer' <statement>

<match-stmt> ::= 'match' '(' <expression> ')' '{' { <match-arm> } '}'

<match-arm> ::= <match-pattern> '=>' <statement> [ ',' ]

<match-pattern> ::= '_'
                  | <identifier> [ '(' <identifier> { ',' <identifier> } ')' ]
                  | <identifier> '::' <identifier> [ '(' <identifier> { ',' <identifier> } ')' ]
                  | <literal-pattern>

<literal-pattern> ::= <integer-literal> | <float-literal> | <string-literal>
                    | <char-literal> | 'true' | 'false'
                    | '-' <integer-literal> | '-' <float-literal>
```

Match statements destructure enum values by variant, or match on scalar/string values using literal patterns. When matching on an enum type, each arm matches a variant pattern and binds payload fields to local variables. Variant names can be unqualified (`Some(v)`) or qualified (`Option::Some(v)`). When matching on integers, floats, strings, chars, or bools, each arm uses a literal pattern. The wildcard pattern `_` matches any value. Match expressions (used as values) require a `_` arm. Commas between arms are optional.

```bnf
<expr-stmt> ::= <expression> ';'
```

---

## Expressions

### Expression (with assignment)

```bnf
<expression> ::= <assignment>

<assignment> ::= <or-expr> [ <assign-op> <assignment> ]

<assign-op> ::= '=' | '+=' | '-=' | '*=' | '/=' | '%='
             | '&=' | '|=' | '^=' | '<<=' | '>>='
```

### Binary Expressions (by precedence, lowest to highest)

```bnf
<or-expr> ::= <and-expr> { '||' <and-expr> }

<and-expr> ::= <bit-or-expr> { '&&' <bit-or-expr> }

<bit-or-expr> ::= <bit-xor-expr> { '|' <bit-xor-expr> }

<bit-xor-expr> ::= <bit-and-expr> { '^' <bit-and-expr> }

<bit-and-expr> ::= <equality-expr> { '&' <equality-expr> }

<equality-expr> ::= <comparison-expr> { ( '==' | '!=' ) <comparison-expr> }

<comparison-expr> ::= <shift-expr> { ( '<' | '>' | '<=' | '>=' ) <shift-expr> }

<shift-expr> ::= <term-expr> { ( '<<' | '>>' ) <term-expr> }

<term-expr> ::= <factor-expr> { ( '+' | '-' ) <factor-expr> }

<factor-expr> ::= <cast-expr> { ( '*' | '/' | '%' ) <cast-expr> }
```

### Cast Expressions

```bnf
<cast-expr> ::= <unary-expr> { 'as' <type> }
```

Cast expressions convert between compatible types. Supported conversions:
- `char` to any integer type (`i8`, `i16`, `i32`, `i64`, `u8`, `u16`, `u32`, `u64`)
- Any integer type to `char`
- Integer to integer (widening or narrowing)
- Simple enum (no payload variants) to integer
- Struct reference to `voidptr` (opaque pointer)
- `voidptr` to `u64` (opaque pointer value for hashing/interop)
- Identity casts (same type to same type)

Examples: `'A' as i32` (yields 65), `65 as char` (yields 'A'), `x as i64`

### Unary Expressions

```bnf
<unary-expr> ::= <unary-op> <unary-expr>
              | <postfix-expr>

<unary-op> ::= '!' | '-' | '~'
```

### Postfix Expressions

```bnf
<postfix-expr> ::= <primary-expr> { <postfix-op> }

<postfix-op> ::= '(' [ <arg-list> ] ')'       (* function call *)
              | '[' <expression> ']'           (* index *)
              | '[' [ <expression> ] ':' [ <expression> ] ']'  (* slice *)
              | '.' <identifier>               (* member access *)
              | '?'                             (* try operator *)

<arg-list> ::= <expression> { ',' <expression> }
```

### Primary Expressions

```bnf
<primary-expr> ::= <int-literal>
                | <float-literal>
                | <string-literal>
                | <interp-string-literal>
                | <char-literal>
                | 'true'
                | 'false'
                | 'null'
                | 'self'
                | <identifier>
                | <enum-value-access>
                | '(' <expression> ')'
                | <new-expr>
                | <tuple-literal>
                | <array-literal>
                | <match-expr>
                | <lambda-expr>

<new-expr> ::= 'new' <type> '{' [ <init-list> ] '}'
            | 'new' <type> '(' [ <arg-list> ] ')'

<init-list> ::= <field-init-list>
             | <element-list>

<enum-value-access> ::= <identifier> '::' <identifier>

<array-literal> ::= '[' [ <expression> { ',' <expression> } [ ',' ] ] ']'

<field-init-list> ::= <field-init> { ',' <field-init> } [ ',' ]

<field-init> ::= <identifier> ':' <expression>

<element-list> ::= <expression> { ',' <expression> } [ ',' ]

<tuple-literal> ::= '(' <expression> ',' <expression> { ',' <expression> } ')'

<match-expr> ::= 'match' '(' <expression> ')' '{' { <match-expr-arm> } '}'

<match-expr-arm> ::= <match-pattern> '=>' <expression> [ ',' ]

<lambda-expr> ::= '|' [ <lambda-params> ] '|' [ '->' <type> ] ( <block> | <expression> )
               | '||' [ '->' <type> ] ( <block> | <expression> )

<lambda-params> ::= <lambda-param> { ',' <lambda-param> }

<lambda-param> ::= <identifier> ':' <type>
```

`match` can be used as a statement or expression. In expression form, each arm body must be an expression (block bodies are not allowed).

**Lambda expressions:** `|x: i64| x * 2` creates an anonymous function. Lambdas have type `func(T1, T2, ...): R` and are represented as fat pointers (`__Closure { fn, env }`). Parameter type annotations are required unless inferred from context. The return type is inferred from the body for expression lambdas, or defaults to `void` for block lambdas (use `-> T` to specify explicitly). Empty-parameter lambdas use `||`: `|| 42`. Lambdas can be assigned to `func(...)` typed variables, passed as function arguments, or called directly: `(|x: i64| x + 1)(42)`.

**Closures:** Lambdas can capture variables from enclosing scopes. Captured variables are copied into a heap-allocated environment struct at lambda creation time. RC-managed captures (strings, structs, vecs) are automatically reference-counted — the environment holds its own reference, which is released when the closure's environment is freed. Non-capturing lambdas have a `NULL` environment (zero overhead). Example: `var offset = 10; var f = |x: i64| -> i64 x + offset;`

**Tuple literals:** `(1, "hello")` creates a tuple value. Tuples must have at least two elements. Access elements by index: `t[0]`, `t[1]`.

**Array literals:** `[1, 2, 3, 4, 5]` creates an array. The element type is inferred from the first element. All elements must have compatible types. Trailing commas are allowed.

**New expressions:** `new Point { x: 1, y: 2 }` heap-allocates a struct with reference counting. `new Vec<i64>{1, 2, 3}` heap-allocates a Vec with initial elements (or `new Vec<i64>{}` for empty). The returned pointer is automatically freed when its reference count drops to zero. Copies of RC pointers (`var q = p`) increment the reference count. RC variables are automatically decremented when they go out of scope.

**Constructor form:** `new Point(1, 2)` heap-allocates a struct (zero-initialized), then calls its `init` method with the provided arguments. This form is required when the struct has an `init` method defined in an inherent `impl` block. The struct literal form `new Type { ... }` is disallowed for types with `init`.

**Slice expressions:** `arr[start:end]` creates a `Span<T>` view into an array, span, or vec. Both bounds are optional:
- `arr[:]` — full span (all elements)
- `arr[1:]` — from index 1 to end
- `arr[:3]` — from start to index 3 (exclusive)
- `arr[1:3]` — from index 1 to 3 (exclusive)

---

## Lexical Elements

### Keywords

```
as       break    by          const     continue  defer
else     enum     extern      false     for       foreach
func     if       impl        import    in        match
new      null     public      private   return    self
struct   test     trait       true      type      use
var      while
```

### Identifiers

```bnf
<identifier> ::= ( <letter> | '_' ) { <letter> | <digit> | '_' }

<letter> ::= 'a' | ... | 'z' | 'A' | ... | 'Z'

<digit> ::= '0' | ... | '9'
```

### Literals

```bnf
<int-literal> ::= <decimal-literal>
               | <hex-literal>
               | <binary-literal>
               | <octal-literal>

<decimal-literal> ::= <digit> { <digit> }

<hex-literal> ::= '0' ( 'x' | 'X' ) <hex-digit> { <hex-digit> }

<binary-literal> ::= '0' ( 'b' | 'B' ) <binary-digit> { <binary-digit> }

<octal-literal> ::= '0' ( 'o' | 'O' ) <octal-digit> { <octal-digit> }

<hex-digit> ::= <digit> | 'a' | ... | 'f' | 'A' | ... | 'F'

<binary-digit> ::= '0' | '1'

<octal-digit> ::= '0' | ... | '7'

<float-literal> ::= <digit> { <digit> } '.' <digit> { <digit> } [ <exponent> ]
                 | <digit> { <digit> } <exponent>

<exponent> ::= ( 'e' | 'E' ) [ '+' | '-' ] <digit> { <digit> }

<string-literal> ::= '"' { <string-char> } '"'

<triple-string-literal> ::= '"""' <newline> { <any-char> } '"""'

<interp-string-literal> ::= '$"' { <interp-part> } '"'

<interp-part> ::= <string-char>
               | '{{' | '}}'
               | '{' <expression> '}'

<string-char> ::= <any-char-except-quote-or-backslash>
               | <string-escape-sequence>

<char-literal> ::= '\'' ( <char-char> | <char-escape-sequence> ) '\''

<char-char> ::= <any-char-except-quote-or-backslash>

<string-escape-sequence> ::= '\\' ( 'n' | 't' | 'r' | '0' | '\\' | '\'' | '"' )

<char-escape-sequence> ::= <string-escape-sequence>
                         | '\\x' <hex-digit> <hex-digit>
                         | '\\' <octal-digit> [ <octal-digit> [ <octal-digit> ] ]
```

Current bootstrap limitation: string/char semantic decoding only handles simple escapes (`\n`, `\t`, `\r`, `\0`, `\\`, `\'`, `\"`). Hex/octal char escapes are tokenized but not numerically decoded yet.

**String interpolation:** `$"Hello {name}!"` embeds expressions inside `{...}` braces. Any expression that resolves to a printable type (`i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `bool`, `char`, `string`) can appear inside braces. Use `{{` and `}}` for literal brace characters. Interpolated strings produce a `string` value.

### Operators and Punctuation

```
+     -     *     /     %     &     |     ^     ~     !
=     <     >
+=    -=    *=    /=    %=    &=    |=    ^=
==    !=    <=    >=    &&    ||    <<    >>    <<=   >>=
=>    ::    ..    ...   ?
(     )     {     }     [     ]     ;     :     ,     .
```

### Comments

```bnf
<line-comment> ::= '//' { <any-char-except-newline> } <newline>

<block-comment> ::= '/*' { <any-char> } '*/'
```

---

## Operator Precedence

From lowest to highest precedence:

| Precedence | Operators                                                | Associativity |
| ---------- | -------------------------------------------------------- | ------------- |
| 1          | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | Right         |
| 2          | `\|\|`                                                   | Left          |
| 3          | `&&`                                                     | Left          |
| 4          | `\|`                                                     | Left          |
| 5          | `^`                                                      | Left          |
| 6          | `&`                                                      | Left          |
| 7          | `==` `!=`                                                | Left          |
| 8          | `<` `>` `<=` `>=`                                        | Left          |
| 9          | `<<` `>>`                                                | Left          |
| 10         | `+` `-`                                                  | Left          |
| 11         | `*` `/` `%`                                              | Left          |
| 12         | `as` (type cast)                                         | Left          |
| 13         | `!` `-` `~` (unary prefix)                               | Right         |
| 14         | `()` `[]` `.` `?` (postfix)                              | Left          |
