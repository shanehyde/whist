# Whist Bootstrap Compiler (w0)

Bootstrap compiler for the Whist programming language, written in C.

## Build

```bash
cc -Wall -o whist main.c lexer.c parser.c ast.c
```

## Usage

```bash
./whist              # Parse built-in test code and print AST
./whist <file.w>     # Parse a source file
./whist --lex        # Lex only (print tokens)
```

## Project Structure

- `lexer.h/c` - Lexer (tokenizer)
- `ast.h/c` - AST node definitions and memory management
- `parser.h/c` - Recursive descent parser
- `main.c` - Test driver with AST printer

## Language Overview

Whist is a C-like language with the following features:

### Keywords

`if`, `else`, `while`, `for`, `return`, `break`, `continue`, `struct`, `enum`, `func`, `var`, `const`, `true`, `false`, `null`

### Literals

- Integers: `42`, `0xFF` (hex), `0b1010` (binary), `0o755` (octal)
- Floats: `3.14`, `1e10`, `2.5e-3`
- Strings: `"hello\n"`
- Characters: `'a'`, `'\n'`

### Operators

- Arithmetic: `+`, `-`, `*`, `/`, `%`
- Comparison: `==`, `!=`, `<`, `>`, `<=`, `>=`
- Logical: `&&`, `||`, `!`
- Bitwise: `&`, `|`, `^`, `~`, `<<`, `>>`
- Assignment: `=`, `+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`
- Other: `++`, `--`, `->`

### Comments

```
// line comment
/* block comment */
```

### Syntax Examples

```
struct Point {
    x: int,
    y: int,
}

func add(a: int, b: int): int {
    return a + b;
}

func main() {
    var x = 42;
    var y: float = 3.14;
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
}
```

## Status

- [x] Lexer
- [x] AST
- [x] Parser
- [ ] Type checker
- [ ] Code generation
