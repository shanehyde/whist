# w0 — Whist Bootstrap Compiler

The bootstrap compiler for the Whist programming language, written in C. Generates C code that can be compiled with any standard C compiler.

The w0 compiler implements the full compilation pipeline: lexer -> parser -> type checker -> C code generator.

## Build & Run

```bash
make              # Build bin/w0
make test         # Run test suite
make format       # Format source files
```

### Usage

```bash
bin/w0 <file.w>                        # Compile to C (stdout)
bin/w0 -o out.c <file.w>               # Compile to C file
bin/w0 --check <file.w>                # Type check only
bin/w0 --ast <file.w>                  # Print AST
bin/w0 --lib-path ../lib <file.w>      # Specify library path
bin/w0 --rc-debug <file.w>              # Generate RC debug info
```

Compile and run a program:

```bash
bin/w0 --lib-path ../lib program.w | cc -x c -I../lib/include -o program - && ./program
```

## Architecture

| File            | Purpose                      |
| --------------- | ---------------------------- |
| `lexer.c/h`     | Tokenization                 |
| `parser.c/h`    | AST construction             |
| `checker.c/h`   | Type checking and validation |
| `codegen.c/h`   | C code generation            |
| `types.c/h`     | Type system implementation   |
| `ast.c/h`       | AST node definitions         |
| `print_ast.c/h` | AST pretty-printing          |

Detailed phase contracts and ownership rules are documented in `ARCHITECTURE.md`.

## Complexity Baseline

Generate a complexity baseline report (NLOC + cyclomatic complexity):

```bash
./scripts/complexity_report.sh
```

## Test Suite

```bash
make test          # Run all tests
make test-valid    # Run valid program tests only
make test-errors   # Run error case tests only
make test-verbose  # Verbose output
```
