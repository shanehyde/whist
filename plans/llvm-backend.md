# LLVM Backend

Native code generation via LLVM for optimized binaries.

## Goals

1. **Native performance** - Direct machine code without C compilation step
2. **Optimizations** - Access to LLVM's optimization passes
3. **Debug info** - Generate DWARF for debugger support
4. **Multiple targets** - x86_64, ARM64, WebAssembly, etc.

## Current State

```
Whist source → w0 → C code → C compiler → binary
```

## Target State

```
Whist source → wc → LLVM IR → LLVM → binary
                 ↘ C code (fallback)
```

## LLVM Integration Options

### Option A: LLVM C API

Use LLVM's C bindings directly:

```whist
import llvm;

func codegen_function(f: FuncDecl): LLVMValueRef {
    var fn_type = llvm.FunctionType(
        return_type: to_llvm_type(f.return_type),
        param_types: f.params.map(|p| to_llvm_type(p.type)),
        is_vararg: false,
    );
    var fn = llvm.AddFunction(module, f.name, fn_type);
    // ...
}
```

**Pros:**
- Direct control
- No external tool dependencies at runtime
- Can be linked statically

**Cons:**
- Complex API
- Must handle LLVM version compatibility
- Large dependency

### Option B: Emit LLVM IR Text

Generate `.ll` files, invoke `llc` and linker:

```
wc → program.ll → llc → program.o → linker → program
```

```whist
func emit_function(f: FuncDecl, out: StringBuilder): void {
    out.append("define ");
    out.append(llvm_type(f.return_type));
    out.append(" @{f.name}(");
    // ...
}
```

**Pros:**
- Simpler implementation
- Human-readable intermediate form
- Easy debugging

**Cons:**
- Slower (parse IR each time)
- External tool dependency
- Text escaping complexity

### Option C: LLVM Bitcode

Generate `.bc` files via C API, use `llc`:

```
wc → program.bc → llc → program.o → linker → program
```

**Pros:**
- Faster than text IR
- Can cache compiled modules
- Binary format is stable

**Cons:**
- Still need llc at runtime
- Binary format harder to debug

## LLVM IR Mapping

### Types

| Whist | LLVM IR |
|-------|---------|
| `void` | `void` |
| `bool` | `i1` |
| `i8` | `i8` |
| `i16` | `i16` |
| `i32` | `i32` |
| `i64` | `i64` |
| `f32` | `float` |
| `f64` | `double` |
| `char` | `i32` (Unicode codepoint) |
| `string` | `{ i8*, i64 }` (ptr + len) |
| `*T` | `T*` |
| `[n]T` | `[n x T]` |
| `struct` | `{ field1, field2, ... }` |

### Function Declaration

```whist
func add(a: i64, b: i64): i64 {
    return a + b;
}
```

```llvm
define i64 @add(i64 %a, i64 %b) {
entry:
    %result = add i64 %a, %b
    ret i64 %result
}
```

### Control Flow

```whist
func max(a: i64, b: i64): i64 {
    if a > b {
        return a;
    }
    return b;
}
```

```llvm
define i64 @max(i64 %a, i64 %b) {
entry:
    %cmp = icmp sgt i64 %a, %b
    br i1 %cmp, label %then, label %else
then:
    ret i64 %a
else:
    ret i64 %b
}
```

### Structs

```whist
struct Point { x: i64, y: i64 }

func origin(): Point {
    return Point { x: 0, y: 0 };
}
```

```llvm
%Point = type { i64, i64 }

define %Point @origin() {
entry:
    ret %Point { i64 0, i64 0 }
}
```

### Method Calls

```whist
func (Point) magnitude(): f64 {
    return sqrt(self.x * self.x + self.y * self.y);
}
```

```llvm
define double @Point_magnitude(%Point* %self) {
entry:
    %x_ptr = getelementptr %Point, %Point* %self, i32 0, i32 0
    %x = load i64, i64* %x_ptr
    %y_ptr = getelementptr %Point, %Point* %self, i32 0, i32 1
    %y = load i64, i64* %y_ptr
    ; ... compute magnitude
}
```

## Debug Information

Generate DWARF debug info for source-level debugging:

```llvm
!llvm.dbg.cu = !{!0}

!0 = distinct !DICompileUnit(language: DW_LANG_C, file: !1, ...)
!1 = !DIFile(filename: "program.w", directory: "/path/to")

define i64 @add(i64 %a, i64 %b) !dbg !2 {
    ; ...
}

!2 = distinct !DISubprogram(name: "add", file: !1, line: 5, ...)
```

This enables:
- Breakpoints on Whist source lines
- Variable inspection in debugger
- Stack traces with Whist function names

## Optimization Passes

LLVM provides optimization pipeline:

```bash
# No optimization
wc program.w -O0

# Basic optimization
wc program.w -O1

# Standard optimization
wc program.w -O2

# Aggressive optimization
wc program.w -O3

# Size optimization
wc program.w -Os
```

Key optimizations:
- Inlining
- Dead code elimination
- Loop unrolling
- Constant propagation
- Tail call optimization
- Vectorization

## Cross Compilation

Target different architectures:

```bash
# Native (default)
wc program.w

# Cross-compile to ARM64
wc program.w --target=aarch64-linux-gnu

# Cross-compile to Windows
wc program.w --target=x86_64-windows-msvc

# WebAssembly
wc program.w --target=wasm32
```

## Implementation Phases

### Phase 1: Basic Codegen

1. Set up LLVM C API bindings
2. Generate IR for simple functions
3. Compile arithmetic expressions
4. Handle basic control flow
5. Output executable via LLVM tools

### Phase 2: Full Language

1. Structs and methods
2. Generics (monomorphize before IR gen)
3. Modules and imports
4. String handling
5. Array operations

### Phase 3: Debug Info

1. Source locations
2. Variable debug info
3. Type debug info
4. Integration with gdb/lldb

### Phase 4: Optimization

1. Wire up optimization passes
2. Add optimization flags
3. Profile-guided optimization
4. Link-time optimization

### Phase 5: Polish

1. Better error messages
2. Incremental compilation
3. Caching of LLVM modules
4. Parallel compilation

## Challenges

### 1. LLVM Version Compatibility

LLVM API changes between versions:
- Target specific LLVM version (e.g., LLVM 17)
- Or maintain compatibility layer
- Document supported versions

### 2. String/Slice Representation

Need to match C ABI for interop:
- Decide on string representation
- Handle fat pointers for slices
- Consider null-termination needs

### 3. Exception Handling

If Whist adds exceptions/panics:
- LLVM has multiple EH models
- Dwarf, SEH, SJLJ
- Or use return-based error handling

### 4. Build System Integration

Users need LLVM installed:
- Bundle LLVM libraries?
- Provide pre-built binaries?
- Fall back to C codegen?

## Open Questions

1. **Which LLVM version to target?**
   - Latest stable (LLVM 17/18)?
   - LTS version?

2. **Link LLVM statically or dynamically?**
   - Static: larger binary, simpler deployment
   - Dynamic: smaller binary, LLVM must be installed

3. **Keep C backend?**
   - Useful for bootstrapping
   - Fallback for unsupported targets
   - Debugging codegen issues

4. **JIT compilation?**
   - LLVM supports JIT via ORC
   - Useful for REPL
   - Add later?

5. **Incremental compilation?**
   - Compile changed functions only
   - Cache LLVM modules
   - Link incrementally

## Example Workflow

```bash
# Compile to LLVM IR (for inspection)
wc program.w --emit=llvm-ir -o program.ll

# Compile to object file
wc program.w --emit=obj -o program.o

# Compile to executable (default)
wc program.w -o program

# Compile with optimizations and debug info
wc program.w -O2 -g -o program

# Cross-compile
wc program.w --target=aarch64-linux-gnu -o program-arm
```

## Related Features

- [Self-Hosting](self-hosting.md) - LLVM backend for wc itself
- [WebAssembly](webassembly.md) - WASM via LLVM
- [Incremental Compilation](incremental-compilation.md) - Cache LLVM modules
