# Whist Bootstrap Compiler (w0)

Bootstrap compiler for the Whist programming language, written in C.
Generates C code that can be compiled with any C compiler (gcc, clang).

## Build

```bash
make          # Build bin/w0
make clean    # Remove build artifacts
make test     # Run test suite
```

---

ai_agent_integration:
visibility: public
purpose: "Provide build, run, and introspection instructions for the w0 compiler and tests"
entrypoint: "bin/w0"
recommended_commands: - "make" - "make test" - "./bin/w0 --ast test/functions.w" - "./bin/w0 --check test/\*.w"
allowed_actions: - "read_files: true" - "run_commands: true" - "modify_docs: true" - "modify_code: false # request explicit permission before changing source code"
primary_paths: - "w0/" - "w0/test/"
notes: |
Agents may run build and test commands to verify changes. If code changes are
required, request explicit user approval before editing source files.

---

**AI Agent Integration**

- **Purpose:** Provide structured metadata so automated agents can discover how to build, test, and inspect the bootstrap compiler.
- **Entrypoint:** `bin/w0` (built by `make`).
- **Recommended checks:** run `make`, then `make test`, and use `bin/w0 --ast` or `--check` for quick inspections.
- **Safety note:** Agents may read and run commands in the workspace. Require explicit user confirmation before modifying source files or committing changes.

## Usage

```bash
bin/w0 <file.w>              # Compile to C (output to stdout)
bin/w0 -o out.c <file.w>     # Compile to C file
bin/w0 --check <file.w>      # Type check only
bin/w0 --parse <file.w>      # Parse only (skip type checking)
bin/w0 --ast <file.w>        # Print AST
bin/w0 --lex <file.w>        # Lex only (print tokens)
```

## Compiling Whist Programs

```bash
# Compile whist to C, then C to executable
bin/w0 -o program.c program.w
cc -o program program.c

# Or in one step
bin/w0 program.w | cc -x c -o program -
```

## Project Structure

```
w0/
├── Makefile       # Build system
├── grammar.md     # BNF grammar specification
├── bin/           # Build output (w0 executable)
├── lexer.h/c      # Lexer (tokenizer)
├── ast.h/c        # AST node definitions and memory management
├── parser.h/c       # Parser entry point and program parsing
├── parser_util.h/c  # Shared parser utilities (advance, match, error, etc.)
├── parse_*.h/c      # Grammar rule implementations:
│   ├── parse_expression  # Expression parsing with precedence climbing
│   ├── parse_primary     # Literals, identifiers, grouping
│   ├── parse_statement   # Statement dispatch
│   ├── parse_block       # Block statements
│   ├── parse_*_stmt      # Control flow (if, for, while, foreach, return)
│   ├── parse_*_decl      # Declarations (func, struct, enum, var)
│   ├── parse_type        # Type annotations
│   └── parse_struct_init # Struct initializers
├── types.h/c      # Type system and type operations
├── checker.h/c    # Type checker with symbol table
├── codegen.h/c    # C code generator
├── main.c           # Compiler driver
├── print_ast.c      # AST pretty-printer
└── test/            # Test programs (.w files)
```

## Running Tests

```bash
make test     # Run all tests

# Or manually:
# Type check all valid programs
for f in test/*.w; do
    case "$f" in test/error_*) continue;; esac
    echo -n "$f: "; bin/w0 --check "$f" 2>&1
done

# Compile and run all valid programs
for f in test/*.w; do
    case "$f" in test/error_*) continue;; esac
    echo -n "$f: "
    bin/w0 -o /tmp/out.c "$f" 2>/dev/null && \
    cc -w -o /tmp/out /tmp/out.c && \
    /tmp/out; echo "exit $?"
done
```

## Language Overview

Whist is a C-like language with the following features:

### Types

- Primitives: `void`, `bool`, `i64`, `i8`, `i16`, `i32`, `u64`, `u8`, `u16`, `u32`, `f32`, `f64`, `char`, `string`
- Pointers: `*T`
- Arrays: `[n]T`
- User-defined: `struct`, `enum`

### Type Mapping to C

| Whist  | C            |
| ------ | ------------ |
| void   | void         |
| bool   | bool         |
| i64    | int64_t      |
| i8     | int8_t       |
| i16    | int16_t      |
| i32    | int32_t      |
| u64    | uint64_t     |
| u8     | uint8_t      |
| u16    | uint16_t     |
| u32    | uint32_t     |
| f32    | float        |
| f64    | double       |
| char   | char         |
| string | const char\* |

### Keywords

`if`, `else`, `while`, `for`, `return`, `break`, `continue`, `struct`, `enum`, `func`, `var`, `const`, `true`, `false`, `null`

### Literals

- Integers: `42`, `0xFF` (hex), `0b1010` (binary), `0o755` (octal)
- Floats: `3.14`, `1e10`, `2.5e-3`
- Strings: `"hello\n"`
- Characters: `'a'`, `'\n'`
- Booleans: `true`, `false`
- Null: `null`

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`
- Pointer: `&` (address-of), `*` (dereference), `->` (member access)
- Other: `++`, `--`, `.`

### Comments

```
// line comment
/* block comment */
```

### Syntax Examples

```
struct Point {
    x: i64,
    y: i64,
}

enum Color {
    Red,
    Green,
    Blue,
}

func add(a: i64, b: i64): i64 {
    return a + b;
}

func main(): i64 {
    var x = 42;
    var y: f32 = 3.14;
    var y64: f64 = 3.14;
    const PI = 3.14159;

    if (x > 0) {
        return x;
    } else {
        return 0;
    }

    for (var i = 0; i < 10; i++) {
        x = x + i;
    }

    while (x > 0) {
        x--;
    }

    return 0;
}
```

## Compiler Phases

1. **Lexer** - Tokenizes source into tokens (keywords, identifiers, literals, operators)
2. **Parser** - Builds AST using recursive descent with precedence climbing
3. **Type Checker** - Validates types, builds symbol table, checks scopes
4. **Code Generator** - Emits C code from AST

## Status

- [x] Lexer
- [x] AST
- [x] Parser
- [x] Type checker
- [x] Code generation (C backend)
