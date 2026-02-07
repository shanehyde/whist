# Self-Hosting (wc)

Rewrite the Whist compiler in Whist itself.

## Goals

1. **Validate the language** - Prove Whist is expressive enough for real systems programming
2. **Dogfooding** - Find pain points and missing features by using the language
3. **Bootstrapping** - Compile the compiler with itself
4. **Milestone** - Major credibility marker for language maturity

## Current State

- `w0`: Bootstrap compiler written in C (~8,150 lines)
- Generates C code compiled by any C compiler
- Feature-complete for current language spec

## Target State

- `wc`: Self-hosted compiler written in Whist
- Can compile itself (bootstrapping)
- Feature parity with w0, then surpass it

## Bootstrapping Strategy

### Stage 0: w0 (C compiler)
```
w0 (C source) → C compiler → w0 binary
w0 binary + program.w → program.c → C compiler → program binary
```

### Stage 1: wc compiled by w0
```
w0 binary + wc.w → wc.c → C compiler → wc-stage1 binary
```

### Stage 2: wc compiled by wc-stage1
```
wc-stage1 + wc.w → wc.c → C compiler → wc-stage2 binary
```

### Stage 3: Verify bootstrap
```
wc-stage2 + wc.w → wc.c (should match stage 1 output)
```

If stage 1 and stage 2 produce identical output, bootstrap is verified.

## Architecture

### Recommended Module Structure

```
wc/
├── src/
│   ├── main.w           # Entry point, CLI
│   ├── lexer.w          # Tokenization
│   ├── parser.w         # Parsing to AST
│   ├── ast.w            # AST node definitions
│   ├── checker.w        # Type checking
│   ├── types.w          # Type system
│   ├── codegen.w        # C code generation
│   ├── symbols.w        # Symbol table
│   └── errors.w         # Error reporting
├── lib/
│   ├── std.w            # Standard library
│   ├── io.w             # File I/O
│   ├── strings.w        # String utilities
│   └── collections.w    # Vec, HashMap, etc.
└── tests/
    └── ...              # Test suite (reuse w0 tests)
```

### Key Data Structures

```whist
// Token representation
struct Token {
    kind: TokenKind,
    lexeme: string,
    line: i32,
    column: i32,
}

enum TokenKind {
    // Literals
    Integer, Float, String, Char,
    // Keywords
    Func, Struct, Enum, Var, Const, ...
    // Operators
    Plus, Minus, Star, Slash, ...
    // Punctuation
    LParen, RParen, LBrace, RBrace, ...
    // Special
    Eof, Error,
}

// AST nodes
enum Expr {
    Literal(Value),
    Identifier(string),
    Binary(Box<Expr>, BinOp, Box<Expr>),
    Unary(UnaryOp, Box<Expr>),
    Call(Box<Expr>, Vec<Expr>),
    Member(Box<Expr>, string),
    Index(Box<Expr>, Box<Expr>),
    ...
}

enum Stmt {
    Var(string, ?Type, Expr),
    Return(?Expr),
    If(Expr, Block, ?Block),
    While(Expr, Block),
    For(string, Expr, Expr, Block),
    ...
}

// Type representation
enum Type {
    Void, Bool,
    I8, I16, I32, I64,
    U8, U16, U32, U64,
    F32, F64,
    Char, String,
    Pointer(Box<Type>),
    Array(Box<Type>, i64),
    Struct(string, Vec<Field>),
    Enum(string, Vec<string>),
    Func(Vec<Type>, Box<Type>),
    Generic(string, Vec<Type>),
}
```

## Implementation Order

### Phase 1: Minimal Viable Compiler

Get something that can compile simple programs:

1. **Lexer** - Tokenize source code
2. **Parser** - Parse to AST (subset of language)
3. **Codegen** - Generate C code (no type checking yet)
4. **Main** - CLI to tie it together

Target: Compile hello world

```whist
import std;

func main(): i32 {
    std.print("Hello from wc!\n");
    return 0;
}
```

### Phase 2: Type System

Add type checking:

1. **Symbol table** - Track declarations
2. **Type checker** - Validate types
3. **Type inference** - Infer variable types
4. **Error reporting** - Helpful error messages

### Phase 3: Full Language

Add remaining features:

1. Methods and receivers
2. Generics and monomorphization
3. Modules and imports
4. Tuples and destructuring
5. Spans and slices
6. Defer statements

### Phase 4: Bootstrap

1. Compile wc with w0
2. Compile wc with wc-stage1
3. Verify identical output
4. Celebrate!

### Phase 5: Improve

Now that we're self-hosted:

1. Better error messages
2. Optimization passes
3. New language features
4. Performance improvements

## Language Features Needed

Features that would make the compiler easier to write:

| Feature | Why Needed | Status |
|---------|------------|--------|
| Union types | AST nodes, Token kinds | Planned |
| Pattern matching | Processing AST | Planned |
| Result/Option | Error handling | Planned |
| String interpolation | Error messages, codegen | Planned |
| HashMap | Symbol tables | Needed |
| Vec (growable array) | Lists of AST nodes | Needed |
| String builder | Code generation | Needed |
| File I/O | Read source, write output | Needed |

## Standard Library Requirements

Minimum stdlib needed for compiler:

```whist
// I/O
func read_file(path: string): Result<string, Error>;
func write_file(path: string, content: string): Result<void, Error>;
func print(s: string): void;
func eprint(s: string): void;  // stderr

// Strings
func (string) length(): i64;
func (string) char_at(i: i64): char;
func (string) substring(start: i64, end: i64): string;
func (string) contains(s: string): bool;
func (string) starts_with(s: string): bool;
func (string) split(sep: string): Vec<string>;

// String builder
struct StringBuilder { ... }
func (StringBuilder) append(s: string): void;
func (StringBuilder) to_string(): string;

// Collections
struct Vec<T> { ... }
func (Vec<T>) push(item: T): void;
func (Vec<T>) pop(): ?T;
func (Vec<T>) get(i: i64): ?T;
func (Vec<T>) len(): i64;

struct HashMap<K, V> { ... }
func (HashMap<K, V>) insert(k: K, v: V): void;
func (HashMap<K, V>) get(k: K): ?V;
func (HashMap<K, V>) contains(k: K): bool;

// Process
func exit(code: i32): void;
func args(): Vec<string>;
```

## Challenges

### 1. Feature Bootstrapping

Some features needed to write the compiler may not exist yet:
- Write compiler subset first
- Add features, then rewrite to use them
- Iterative bootstrap

### 2. Error Recovery

Compiler needs good error handling:
- Can't panic on malformed input
- Need to report multiple errors
- Recovery and continue parsing

### 3. Performance

Compiler should be reasonably fast:
- Efficient data structures
- Minimize allocations
- Consider incremental compilation later

### 4. Testing

Reuse w0 test suite:
- Same test files should produce same output
- Compare C output between w0 and wc
- Bootstrap verification

## Metrics

Track progress with:

- Lines of Whist code in wc
- Number of w0 tests passing
- Compile time for self-compilation
- Binary size comparison

## Open Questions

1. **Start from scratch or port w0?**
   - Fresh design might be cleaner
   - Porting ensures compatibility

2. **Same architecture as w0?**
   - Single-pass vs multi-pass
   - AST vs direct codegen

3. **When to switch to wc as primary?**
   - After bootstrap verified
   - After feature parity
   - After performance parity

4. **Maintain w0 or deprecate?**
   - Keep for bootstrapping from scratch
   - Or trust wc binaries

## Timeline Considerations

Rough ordering (not time estimates):

1. Design wc architecture
2. Implement minimal stdlib
3. Lexer + parser for subset
4. Basic codegen
5. Type checker
6. Remaining language features
7. First bootstrap attempt
8. Bug fixes and compatibility
9. Bootstrap verified
10. Ongoing improvements

## Related Features

- [Memory Management](memory-management.md) - How wc manages AST memory
- [Result/Option](result-option.md) - Error handling in compiler
- [Union Types](union-types.md) - AST node representation
- [Pattern Matching](pattern-matching.md) - Processing AST nodes
