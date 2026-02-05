# Debugger Support

Debug info generation and tooling for source-level debugging.

## Goals

1. **Source-level debugging** - Breakpoints on Whist source lines
2. **Variable inspection** - See Whist variables, not C internals
3. **Stack traces** - Whist function names and locations
4. **IDE integration** - VS Code, LLDB, GDB support
5. **Standard tools** - Work with existing debuggers

## Debug Strategies

### Strategy A: Debug via C (Current)

Use C debugger on generated code with source mapping:

```
program.w → w0 → program.c → gcc -g → program (with DWARF)
                     ↓
            Source map (program.w ↔ program.c)
```

Debugger shows C code, but source map helps correlate.

### Strategy B: DWARF for Whist

Generate DWARF debug info referencing Whist source directly:

```
program.w → wc → program.c → gcc -g → program
                     ↓
            DWARF pointing to program.w
```

Debugger shows Whist code directly.

### Strategy C: LLVM Debug Info

With LLVM backend, emit debug info natively:

```
program.w → wc (LLVM) → LLVM IR + debug → binary
                              ↓
                    DWARF pointing to program.w
```

Best experience, requires LLVM backend.

## DWARF Debug Info

### Compile Units

```
DW_TAG_compile_unit
    DW_AT_name: "program.w"
    DW_AT_language: DW_LANG_Whist (or DW_LANG_C)
    DW_AT_producer: "wc 0.1.0"
```

### Types

```whist
struct Point { x: i64, y: i64 }
```

```
DW_TAG_structure_type
    DW_AT_name: "Point"
    DW_AT_byte_size: 16
    DW_TAG_member
        DW_AT_name: "x"
        DW_AT_type: <i64>
        DW_AT_data_member_location: 0
    DW_TAG_member
        DW_AT_name: "y"
        DW_AT_type: <i64>
        DW_AT_data_member_location: 8
```

### Functions

```whist
func add(a: i64, b: i64): i64 {
    return a + b;
}
```

```
DW_TAG_subprogram
    DW_AT_name: "add"
    DW_AT_decl_file: "program.w"
    DW_AT_decl_line: 1
    DW_AT_type: <i64>
    DW_TAG_formal_parameter
        DW_AT_name: "a"
        DW_AT_type: <i64>
        DW_AT_location: (DW_OP_fbreg -16)
    DW_TAG_formal_parameter
        DW_AT_name: "b"
        DW_AT_type: <i64>
        DW_AT_location: (DW_OP_fbreg -8)
```

### Line Number Mapping

```
.debug_line section:
    program.w:1 → 0x1000
    program.w:2 → 0x1010
    program.w:3 → 0x1018
```

## Source Maps

For C backend, generate source map:

```json
{
    "version": 1,
    "sources": ["program.w"],
    "generated": "program.c",
    "mappings": [
        { "whist": { "file": "program.w", "line": 5, "column": 0 },
          "c": { "line": 23, "column": 0 } },
        { "whist": { "file": "program.w", "line": 6, "column": 4 },
          "c": { "line": 24, "column": 4 } }
    ]
}
```

### Compiler Flags

```bash
# Generate debug info
wc -g program.w -o program

# Generate debug info + source map
wc -g --source-map program.w -o program

# Debug info with optimization (may affect accuracy)
wc -g -O2 program.w -o program
```

## GDB/LLDB Integration

### Basic Commands

```bash
$ lldb ./program
(lldb) breakpoint set --file program.w --line 10
(lldb) run
(lldb) bt                    # backtrace
(lldb) frame variable        # local variables
(lldb) print x               # print variable
(lldb) step                  # step into
(lldb) next                  # step over
(lldb) continue              # continue
```

### Expected Output

```
* thread #1, queue = 'com.apple.main-thread', stop reason = breakpoint 1.1
    frame #0: 0x0000000100001234 program`add(a=5, b=3) at program.w:2
   1    func add(a: i64, b: i64): i64 {
-> 2        return a + b;
   3    }
```

### Custom Pretty Printers

For complex types (Vec, HashMap, etc.):

```python
# whist_pretty_printers.py (for GDB)
class VecPrinter:
    def __init__(self, val):
        self.val = val

    def to_string(self):
        length = int(self.val['len'])
        data = self.val['data']
        items = [str(data[i]) for i in range(min(length, 10))]
        if length > 10:
            items.append(f"... ({length - 10} more)")
        return f"Vec [{', '.join(items)}]"
```

```
(gdb) source whist_pretty_printers.py
(gdb) print my_vec
Vec [1, 2, 3, 4, 5]
```

## VS Code Integration

### Debug Configuration

```json
// .vscode/launch.json
{
    "version": "0.2.0",
    "configurations": [
        {
            "name": "Debug Whist",
            "type": "lldb",
            "request": "launch",
            "program": "${workspaceFolder}/bin/program",
            "args": [],
            "cwd": "${workspaceFolder}",
            "preLaunchTask": "build-debug",
            "sourceMap": {
                "${workspaceFolder}/build/*.c": "${workspaceFolder}/src/*.w"
            }
        }
    ]
}
```

### Debug Adapter Protocol

For richer integration, implement DAP:

```
┌─────────────┐      DAP       ┌─────────────┐     MI/LLDB    ┌─────────┐
│   VS Code   │ ←───────────→ │  whist-dap  │ ←────────────→ │  LLDB   │
│             │   (JSON-RPC)   │  (adapter)  │                │         │
└─────────────┘                └─────────────┘                └─────────┘
```

DAP messages:

```json
// Set breakpoint
{
    "command": "setBreakpoints",
    "arguments": {
        "source": { "path": "/path/to/program.w" },
        "breakpoints": [{ "line": 10 }]
    }
}

// Variables request
{
    "command": "variables",
    "arguments": { "variablesReference": 1 }
}

// Response
{
    "body": {
        "variables": [
            { "name": "x", "value": "42", "type": "i64" },
            { "name": "p", "value": "Point { x: 1, y: 2 }", "type": "Point" }
        ]
    }
}
```

## Stack Traces

### Name Mangling

Ensure readable function names:

```whist
func (Point) magnitude(): f64 { ... }
```

Mangle to: `Point_magnitude` (not `_ZN5Point9magnitudeEv`)

### Backtrace Output

```
(lldb) bt
* thread #1, stop reason = breakpoint
  * frame #0: program`calculate(n=10) at math.w:15
    frame #1: program`process(data=...) at process.w:42
    frame #2: program`main() at main.w:8
```

Not:
```
  * frame #0: program`calculate_123abc(n=10) at program.c:892
    frame #1: program`process_456def(data=...) at program.c:1547
```

## Variable Display

### Primitive Types

```
(lldb) print x
(i64) x = 42

(lldb) print pi
(f64) pi = 3.14159

(lldb) print flag
(bool) flag = true

(lldb) print c
(char) c = 'A'
```

### Struct Types

```
(lldb) print point
(Point) point = {
    x = 10
    y = 20
}
```

### Arrays and Spans

```
(lldb) print arr
([5]i64) arr = [1, 2, 3, 4, 5]

(lldb) print span
(Span<i64>) span = {
    data = 0x7fff5fbff8c0
    count = 10
}
```

### Pointers

```
(lldb) print ptr
(*Point) ptr = 0x7fff5fbff8d0 → {
    x = 1
    y = 2
}
```

### Generic Types

```
(lldb) print vec
(Vec<i64>) vec = {
    data = 0x100200300
    len = 5
    cap = 8
    items = [1, 2, 3, 4, 5]
}

(lldb) print map
(HashMap<string, i64>) map = {
    len = 3
    entries = {
        "one": 1,
        "two": 2,
        "three": 3
    }
}
```

## Implementation Phases

### Phase 1: Basic Debug Info

1. Generate line number mapping
2. Generate basic type info
3. Works with C debugger on generated code

### Phase 2: Source Mapping

1. Create source map file
2. VS Code extension uses source map
3. Map breakpoints between files

### Phase 3: Native Debug Info

1. Generate DWARF in C output (via comments/pragmas)
2. Or generate with LLVM backend
3. Direct Whist source debugging

### Phase 4: Rich Tooling

1. DAP adapter for VS Code
2. Pretty printers for stdlib types
3. Expression evaluation in debugger

## Challenges

### 1. Optimization vs Debug

Optimized code may:
- Inline functions (no stack frame)
- Reorder instructions (confusing stepping)
- Eliminate variables (can't inspect)

Solutions:
- `-O0` for debugging
- `-Og` for debug-friendly optimization
- Document limitations

### 2. Generic Monomorphization

`Vec<i64>` and `Vec<string>` are different types:
- Generate debug info for each instantiation
- Or show as generic with type parameter

### 3. Generated Code Mapping

Some Whist constructs expand to complex C:
- `defer` becomes goto/labels
- Pattern matching becomes if/switch chains
- May need synthetic line mappings

### 4. Method Names

Whist methods need readable names:
- `(Point).magnitude()` → `Point_magnitude`
- Must handle generics: `(Vec<T>).push()` → `Vec_i64_push`

## Open Questions

1. **C backend debug info?**
   - Generate DWARF pragmas in C
   - Or just use source maps

2. **DAP vs native debugger?**
   - DAP gives IDE control
   - Native debugger is simpler

3. **Expression evaluation?**
   - Evaluate Whist expressions in debugger
   - Or just show variable values

4. **Conditional breakpoints?**
   - In Whist syntax
   - Or target debugger syntax

5. **Hot reload?**
   - Edit code while debugging
   - Recompile and continue

## Examples

### VS Code Debug Session

```
1. Open program.w in VS Code
2. Set breakpoint on line 15
3. Press F5 (Start Debugging)
4. Program runs and stops at line 15
5. Hover over variables to see values
6. Use Debug Console:
   > p.x
   10
   > p.magnitude()
   14.142135
7. Step through code with F10/F11
8. View call stack in sidebar
```

### Command Line Session

```bash
$ wc -g program.w -o program
$ lldb ./program

(lldb) b main.w:15
Breakpoint 1: where = program`process + 48 at main.w:15

(lldb) r
Process launched...
Stopped at breakpoint 1

(lldb) frame variable
(i64) x = 42
(Point) p = (x = 10, y = 20)

(lldb) p p.x + p.y
(i64) $0 = 30

(lldb) bt
* frame #0: program`process at main.w:15
  frame #1: program`main at main.w:5

(lldb) c
Process exited with status 0
```

## Related Features

- [LLVM Backend](llvm-backend.md) - Native debug info
- [LSP Server](lsp-server.md) - IDE integration
- [Better Error Messages](error-messages.md) - Debugging compiler errors
