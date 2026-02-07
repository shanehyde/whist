# REPL

Interactive Read-Eval-Print Loop for Whist.

## Goals

1. **Quick prototyping** - Test ideas without creating files
2. **Learning tool** - Explore language features interactively
3. **Debugging aid** - Inspect values and test expressions
4. **Expression evaluation** - Calculator-like usage

## Basic Usage

```
$ whist repl
Whist 0.1.0 REPL
Type :help for help, :quit to exit

>>> 1 + 2
3

>>> var x = 42
>>> x * 2
84

>>> func square(n: i64): i64 { return n * n; }
>>> square(5)
25

>>> struct Point { x: i64, y: i64 }
>>> var p = Point { x: 3, y: 4 }
>>> p.x * p.x + p.y * p.y
25
```

## Features

### Expression Evaluation

```
>>> 2 + 3 * 4
14

>>> "hello" + " " + "world"
"hello world"

>>> [1, 2, 3, 4, 5].map(|x| x * 2)
[2, 4, 6, 8, 10]
```

### Variable Persistence

Variables persist across inputs:

```
>>> var count = 0
>>> count += 1
>>> count += 1
>>> count
2
```

### Function Definitions

```
>>> func fib(n: i64): i64 {
...     if n <= 1 { return n; }
...     return fib(n - 1) + fib(n - 2);
... }

>>> fib(10)
55
```

### Type Definitions

```
>>> struct Vec2 { x: f64, y: f64 }

>>> func (Vec2) length(): f64 {
...     return sqrt(self.x * self.x + self.y * self.y);
... }

>>> var v = Vec2 { x: 3.0, y: 4.0 }
>>> v.length()
5.0
```

### Import Modules

```
>>> import std
>>> std.print("Hello!\n")
Hello!

>>> import math
>>> math.sin(math.PI / 2.0)
1.0
```

## REPL Commands

Commands start with `:`:

```
:help           Show help
:quit, :q       Exit REPL
:clear          Clear screen
:reset          Reset all state
:type <expr>    Show type of expression
:ast <expr>     Show AST of expression
:history        Show command history
:save <file>    Save session to file
:load <file>    Load and execute file
:env            Show defined variables/functions
:time <expr>    Time expression evaluation
```

### Examples

```
>>> :type 1 + 2
i64

>>> :type |x: i64| x * 2
func(i64): i64

>>> :ast 1 + 2 * 3
Binary(
  Add,
  Literal(1),
  Binary(Mul, Literal(2), Literal(3))
)

>>> :env
Variables:
  x: i64 = 42
  p: Point = Point { x: 3, y: 4 }

Functions:
  square(i64): i64
  fib(i64): i64

Types:
  Point { x: i64, y: i64 }

>>> :time fib(30)
832040
Time: 45ms
```

## Multi-line Input

Automatic continuation for incomplete expressions:

```
>>> func factorial(n: i64): i64 {
...     if n <= 1 {
...         return 1;
...     }
...     return n * factorial(n - 1);
... }

>>> var matrix = [
...     [1, 2, 3],
...     [4, 5, 6],
...     [7, 8, 9],
... ]
```

Explicit continuation with `\`:

```
>>> var long_string = "This is a very " + \
...     "long string that spans " + \
...     "multiple lines"
```

## Pretty Printing

Format output nicely:

```
>>> var user = User {
...     name: "Alice",
...     age: 30,
...     emails: ["alice@example.com", "alice@work.com"],
... }
User {
    name: "Alice",
    age: 30,
    emails: [
        "alice@example.com",
        "alice@work.com",
    ],
}

>>> HashMap::from([("a", 1), ("b", 2), ("c", 3)])
{
    "a": 1,
    "b": 2,
    "c": 3,
}
```

## Tab Completion

```
>>> std.<TAB>
std.print       std.println     std.abs_i64
std.max_i64     std.min_i64     std.format

>>> var p = Point { x: 1, y: 2 }
>>> p.<TAB>
p.x     p.y     p.length()

>>> :lo<TAB>
:load
```

## History

Navigate with arrow keys:

- ↑/↓ - Previous/next command
- Ctrl+R - Reverse search
- Ctrl+P/N - Previous/next (alternative)

Persistent history:

```
~/.whist/repl_history
```

## Error Handling

Show helpful errors without crashing:

```
>>> 1 + "hello"
Error: Type mismatch
  Expected: i64
  Found: string

>>> undefined_var
Error: Undefined variable 'undefined_var'

>>> func bad() { return }
Error: Parse error at line 1, column 20
  Expected expression after 'return'
```

## Implementation

### Architecture

```
┌─────────────────────────────────────────────────┐
│                    REPL Loop                     │
├──────────┬──────────┬──────────┬────────────────┤
│  Read    │  Parse   │  Eval    │  Print         │
│  (line   │  (check  │  (JIT or │  (format       │
│  editor) │  if cmd) │  interp) │  result)       │
└──────────┴──────────┴──────────┴────────────────┘
                          │
                          ▼
                   ┌─────────────┐
                   │  Persistent │
                   │  State      │
                   │  (vars,     │
                   │   funcs,    │
                   │   types)    │
                   └─────────────┘
```

### Execution Strategy

#### Option A: Interpreter

Build a tree-walking interpreter:

```whist
func eval(expr: Expr, env: Environment): Value {
    match expr {
        Literal(v) => v,
        Identifier(name) => env.get(name),
        Binary(op, left, right) => {
            var l = eval(left, env);
            var r = eval(right, env);
            apply_op(op, l, r)
        },
        Call(func, args) => {
            var f = eval(func, env);
            var evaluated_args = args.map(|a| eval(a, env));
            call_function(f, evaluated_args)
        },
        // ...
    }
}
```

**Pros:** Simple, easy to debug, good error messages
**Cons:** Slow for compute-heavy code

#### Option B: JIT Compilation

Compile to native code on the fly:

```whist
func repl_eval(input: string, state: ReplState): Result<Value, Error> {
    var ast = parse(input)?;
    var typed = type_check(ast, state.env)?;

    // Compile to native code
    var code = jit_compile(typed);

    // Execute
    var result = code.execute(state.runtime);

    return Ok(result);
}
```

**Pros:** Fast execution
**Cons:** Complex, slower startup per expression

#### Option C: Compile to C, Execute

Similar to current w0 approach:

```whist
func repl_eval(input: string, state: ReplState): Result<Value, Error> {
    var ast = parse(input)?;
    var c_code = generate_c(ast, state);

    // Compile and load as shared library
    write_file("/tmp/repl_expr.c", c_code);
    exec("cc", ["-shared", "-o", "/tmp/repl_expr.so", "/tmp/repl_expr.c"]);

    var lib = dlopen("/tmp/repl_expr.so");
    var result = lib.call("eval_expr");

    return Ok(result);
}
```

**Pros:** Reuses existing codegen
**Cons:** Slow for quick expressions

### Persistent State

```whist
struct ReplState {
    // Type environment
    types: HashMap<string, TypeDef>,

    // Value environment
    variables: HashMap<string, Value>,

    // Function definitions
    functions: HashMap<string, FuncDef>,

    // Import context
    imports: Vec<Module>,

    // History
    history: Vec<string>,
}

impl ReplState {
    func new(): ReplState {
        var state = ReplState::default();
        // Pre-import standard library
        state.imports.push(load_module("std"));
        return state;
    }

    func eval(self, input: string): Result<Value, Error> {
        // Parse
        var ast = parse_repl_input(input)?;

        // Update state based on input type
        match ast {
            VarDecl(name, value) => {
                var v = self.eval_expr(value)?;
                self.variables.insert(name, v);
                Ok(Value::Unit)
            },
            FuncDecl(f) => {
                self.functions.insert(f.name, f);
                Ok(Value::Unit)
            },
            TypeDecl(t) => {
                self.types.insert(t.name, t);
                Ok(Value::Unit)
            },
            Expr(e) => self.eval_expr(e),
        }
    }
}
```

## Line Editor

Use a line editing library or implement:

- Basic editing (insert, delete, cursor movement)
- History navigation
- Tab completion
- Syntax highlighting
- Parenthesis matching
- Multi-line editing

```whist
struct LineEditor {
    buffer: String,
    cursor: usize,
    history: Vec<String>,
    history_index: usize,
}

impl LineEditor {
    func read_line(prompt: string): string {
        self.buffer.clear();
        self.cursor = 0;

        loop {
            print(prompt);
            print(self.buffer);
            move_cursor(self.cursor);

            var key = read_key();
            match key {
                Enter => {
                    self.history.push(self.buffer.clone());
                    return self.buffer;
                },
                Backspace => self.delete_char(),
                Left => self.cursor = max(0, self.cursor - 1),
                Right => self.cursor = min(self.buffer.len(), self.cursor + 1),
                Up => self.history_prev(),
                Down => self.history_next(),
                Tab => self.complete(),
                Char(c) => self.insert_char(c),
                Ctrl('C') => return "",
                Ctrl('D') => exit(0),
                _ => {},
            }
        }
    }
}
```

## Challenges

### 1. Incremental Type Checking

Need to type-check new definitions with existing context:

```
>>> var x = 42      // x: i64 added to environment
>>> var y = x + 1   // must know x: i64
```

### 2. Mutable Definitions

What happens when you redefine something?

```
>>> func foo(): i64 { return 1; }
>>> foo()
1
>>> func foo(): i64 { return 2; }  // redefine?
>>> foo()
2  // or error?
```

Options:
- Allow shadowing (new definition hides old)
- Error on redefinition
- Warning but allow

### 3. Side Effects

Handle I/O and mutable state:

```
>>> var count = 0
>>> func increment(): void { count += 1; }
>>> increment()
>>> increment()
>>> count
2
```

### 4. Large Results

Truncate large outputs:

```
>>> (0..1000000).collect()
[0, 1, 2, 3, 4, ... (999995 more elements)]
```

## Open Questions

1. **Execution model?**
   - Interpreter (simple, slower)
   - JIT (fast, complex)
   - Compile-and-run (middle ground)

2. **Allow redefinitions?**
   - Yes (more flexible)
   - No (more predictable)

3. **Implicit printing?**
   - Print expression results automatically
   - Require explicit print

4. **Async support?**
   - Block on async expressions
   - Special async REPL mode

5. **Debugger integration?**
   - Breakpoints in REPL code
   - Step through expressions

## Examples

### Quick Calculations

```
>>> 2 ** 10
1024

>>> (1..=100).sum()
5050

>>> "hello".chars().filter(|c| c == 'l').count()
2
```

### Prototyping

```
>>> struct Todo { id: i64, text: string, done: bool }

>>> var todos: Vec<Todo> = []

>>> func add_todo(text: string): void {
...     todos.push(Todo {
...         id: todos.len() + 1,
...         text: text,
...         done: false,
...     });
... }

>>> add_todo("Learn Whist")
>>> add_todo("Build something cool")

>>> todos.filter(|t| !t.done).map(|t| t.text)
["Learn Whist", "Build something cool"]
```

### Exploring APIs

```
>>> import http

>>> :type http.get
func(string): Result<Response, Error>

>>> var resp = http.get("https://api.example.com/data")?
>>> resp.status
200

>>> resp.json()?
{
    "users": [
        { "name": "Alice", "id": 1 },
        { "name": "Bob", "id": 2 },
    ]
}
```

## Related Features

- [LSP Server](lsp-server.md) - Share analysis infrastructure
- [Self-Hosting](self-hosting.md) - REPL written in Whist
- [Better Error Messages](error-messages.md) - Helpful REPL errors
