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
<program> ::= { <import-stmt> | <declaration> }

<declaration> ::= [ 'public' | 'private' ] <func-defn>
               | [ 'public' | 'private' ] <struct-decl>
               | [ 'public' | 'private' ] <enum-decl>
               | [ 'public' | 'private' ] <var-decl>
               | <extern-module>
```

**Visibility:** Top-level declarations are private by default (file-local scope). The `public` keyword gives a declaration external linkage. In generated C code, private declarations are prefixed with `static`. The `main` function is always treated as having external linkage regardless of the `public` modifier.

---

## Declarations

### Import Statement

```bnf
<import-stmt> ::= 'import' ( <identifier> | <string-literal> ) ';'
```

Import statements load declarations from external Whist source files. There are two forms:

- **Module import:** `import std;` resolves the module name from the `lib/` directory (e.g., `lib/std.w`). Symbols from module imports must be accessed with module qualification: `std.print("hello")`. Unqualified access is an error.
- **Relative import:** `import "./path/to/file.w";` or `import "../file.w";` resolves the path relative to the importing file's directory. String imports must start with `./` or `../`. Symbols from relative imports are merged into the current module and accessed without qualification.

### Function Declaration

```bnf
<func-decl> ::= 'func' [ <receiver> ] <identifier> '(' [ <param-list> ] ')' [ ':' <type> ]
<func-defn> ::= <func-decl> '{' <block> '}'

<receiver> ::= '(' [ 'const' ] <identifier> [ '<' <type-arg-list> '>' ] ')'

<param-list> ::= <param> { ',' <param> } [ ',' '...' ]
               | '...'

<param> ::= <identifier> [ ':' <type> ]

<extern-module> ::= 'extern' <identifier> '{' { <func-decl> ';' } '}'
```

**Generic methods:** Methods can be defined on generic structs using type arguments in the receiver:
- `func (Box<T>) get(): T` — method on any `Box<T>` instantiation
- `func (Pair<i32, Box<T>>) set(v: Box<T>): void` — partially specialized method

### Struct Declaration

```bnf
<struct-decl> ::= 'struct' <identifier> [ '<' <type-param-list> '>' ] '{' { <field-decl> } '}'

<type-param-list> ::= <identifier> { ',' <identifier> }

<field-decl> ::= <identifier> ':' <type> [ ',' ]
```

**Generic structs:** Structs can be parameterized by one or more type parameters:
- `struct Box<T> { value: T }` — single type parameter
- `struct Pair<K, V> { key: K, value: V }` — multiple type parameters

Generic structs are monomorphized at compile time, generating specialized C code for each instantiation (e.g., `Box<i64>` becomes `Box_i64`).

### Enum Declaration

```bnf
<enum-decl> ::= 'enum' <identifier> '{' { <enum-value> } '}'

<enum-value> ::= <identifier> [ ',' ]
```

### Variable Declaration

```bnf
<var-decl> ::= ( 'var' | 'const' ) <identifier> [ ':' <type> ] [ '=' <expression> ] ';'
            | 'var' <destruct-pattern> '=' <expression> ';'

<destruct-pattern> ::= '(' <destruct-element> ',' <destruct-element> { ',' <destruct-element> } ')'

<destruct-element> ::= <identifier>
                    | <destruct-pattern>
```

**Tuple destructuring:** `var (a, b) = expr;` unpacks a tuple into individual variables. The number of elements must match the tuple's arity. Nested patterns are supported: `var (x, (y, z)) = (1, (2, 3));` unpacks nested tuples.

---

## Types

```bnf
<type> ::= <identifier>
        | <identifier> '<' <type-arg-list> '>'
        | '*' <type>
        | '[' [ <expression> ] ']' <type>
        | '(' <type> ',' <type> { ',' <type> } ')'

<type-arg-list> ::= <type> { ',' <type> }
```

**Built-in types** are resolved as identifiers: `void`, `bool`, `i8`–`i64`, `u8`–`u64`, `f32`, `f64`, `char`, `string`, `voidptr`. `voidptr` maps to `void*` in C and is used for opaque pointer handles. It supports `null` assignment and equality comparison (`==`/`!=`) with `null`, but no arithmetic.

**Generic types:** `Box<i64>` or `Pair<i32, string>` instantiate a generic struct with concrete type arguments. Nested generics are supported: `Box<Pair<i32, i64>>`.

**Tuple types:** `(T1, T2)` or `(T1, T2, T3, ...)` represent fixed-size heterogeneous collections. Tuples must have at least two elements.

**Span types:** `Span<T>` is a builtin generic type representing an immutable view into contiguous memory (array or another span). Spans have:
- `span.count` — number of elements (read-only)
- `span[i]` — bounds-checked element access (panics if out of bounds)
- `span.data` — private (compile error if accessed directly)

Create spans using slice syntax: `var s: Span<i64> = arr[:];`

---

## Statements

```bnf
<statement> ::= <var-decl>
             | <if-stmt>
             | <while-stmt>
             | <for-stmt>
             | <foreach-stmt>
             | <return-stmt>
             | <break-stmt>
             | <continue-stmt>
             | <defer-stmt>
             | <block>
             | <expr-stmt>

<block> ::= '{' { <statement> } '}'

<if-stmt> ::= 'if' '(' <expression> ')' '{' <block> '}' [ 'else' ( <if-stmt> | '{' <block> '}' ) ]

<while-stmt> ::= 'while' '(' <expression> ')' '{' <block> '}'

<for-stmt> ::= 'for' '(' [ <for-init> ] ';' [ <expression> ] ';' [ <expression> ] ')' '{' <block> '}'

<for-init> ::= <var-decl>
            | <expression>

<foreach-stmt> ::= 'foreach' '(' 'const' <identifier> 'in' <expression> '..' <expression> [ 'by' <expression> ] ')' '{' <block> '}'

<return-stmt> ::= 'return' [ <expression> ] ';'

<break-stmt> ::= 'break' ';'

<continue-stmt> ::= 'continue' ';'

<defer-stmt> ::= 'defer' <statement>

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

<factor-expr> ::= <unary-expr> { ( '*' | '/' | '%' ) <unary-expr> }
```

### Unary Expressions

```bnf
<unary-expr> ::= <unary-op> <unary-expr>
              | <postfix-expr>

<unary-op> ::= '!' | '-' | '~' | '&' | '*'
```

### Postfix Expressions

```bnf
<postfix-expr> ::= <primary-expr> { <postfix-op> }

<postfix-op> ::= '(' [ <arg-list> ] ')'       (* function call *)
              | '[' <expression> ']'           (* index *)
              | '[' [ <expression> ] ':' [ <expression> ] ']'  (* slice *)
              | '.' <identifier>               (* member access *)
              | '->' <identifier>              (* pointer member access *)

<arg-list> ::= <expression> { ',' <expression> }
```

### Primary Expressions

```bnf
<primary-expr> ::= <int-literal>
                | <float-literal>
                | <string-literal>
                | <char-literal>
                | 'true'
                | 'false'
                | 'null'
                | 'self'
                | <identifier>
                | <enum-value-access>
                | '(' <expression> ')'
                | <struct-init>
                | <tuple-literal>
                | <array-literal>

<enum-value-access> ::= <identifier> '::' <identifier>

<array-literal> ::= '[' [ <expression> { ',' <expression> } [ ',' ] ] ']'

<struct-init> ::= '{' [ <field-init-list> ] '}'

<field-init-list> ::= <field-init> { ',' <field-init> } [ ',' ]

<field-init> ::= <identifier> ':' <expression>

<tuple-literal> ::= '(' <expression> ',' <expression> { ',' <expression> } ')'
```

**Tuple literals:** `(1, "hello")` creates a tuple value. Tuples must have at least two elements. Access elements by index: `t[0]`, `t[1]`.

**Array literals:** `[1, 2, 3, 4, 5]` creates an array. The element type is inferred from the first element. All elements must have compatible types. Trailing commas are allowed.

**Slice expressions:** `arr[start:end]` creates a `Span<T>` view into an array or span. Both bounds are optional:
- `arr[:]` — full span (all elements)
- `arr[1:]` — from index 1 to end
- `arr[:3]` — from start to index 3 (exclusive)
- `arr[1:3]` — from index 1 to 3 (exclusive)

---

## Lexical Elements

### Keywords

```
break    const    continue    defer     else      enum
extern   false    for         foreach   func      if
import   in       null        public    private   return
self     struct   true        var       while
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

<string-char> ::= <any-char-except-quote-or-backslash>
               | <escape-sequence>

<char-literal> ::= '\'' ( <char-char> | <escape-sequence> ) '\''

<char-char> ::= <any-char-except-quote-or-backslash>

<escape-sequence> ::= '\\' ( 'n' | 't' | 'r' | '0' | '\\' | '\'' | '"' )
                   | '\\x' <hex-digit> <hex-digit>
                   | '\\' <octal-digit> [ <octal-digit> [ <octal-digit> ] ]
```

### Operators and Punctuation

```
+     -     *     /     %     &     |     ^     ~     !
=     <     >
+=    -=    *=    /=    %=    &=    |=    ^=
==    !=    <=    >=    &&    ||    <<    >>    <<=   >>=
->    ::    ..
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
| 12         | `!` `-` `~` `&` `*` (unary prefix)                       | Right         |
| 13         | `()` `[]` `.` `->` (postfix)                             | Left          |
