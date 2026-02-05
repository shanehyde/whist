# WebAssembly Target

Compile Whist to WebAssembly for browser and portable execution.

## Goals

1. **Browser execution** - Run Whist programs in web browsers
2. **Sandboxed environments** - WASM provides secure isolation
3. **Portable binaries** - Single binary runs anywhere with WASM runtime
4. **Performance** - Near-native speed in browsers

## Compilation Paths

### Path A: Via LLVM

```
Whist → LLVM IR → LLVM wasm32 backend → .wasm
```

Requires LLVM backend (see [llvm-backend.md](llvm-backend.md)).

### Path B: Via C + Emscripten

```
Whist → C code → Emscripten → .wasm + .js
```

Works with current w0 compiler.

### Path C: Direct WASM Generation

```
Whist → WASM bytecode directly
```

Custom backend, most control but most work.

## WebAssembly Basics

### WASM Types

| Whist | WASM |
|-------|------|
| `i32` | `i32` |
| `i64` | `i64` |
| `f32` | `f32` |
| `f64` | `f64` |
| `bool` | `i32` (0 or 1) |
| `*T` | `i32` (memory index) |

WASM has only 4 value types. Everything else is memory.

### Memory Model

WASM has linear memory (byte array):

```
┌─────────────────────────────────────────────┐
│ Stack │ Heap →                    ← Globals │
└─────────────────────────────────────────────┘
0       SP                                   max
```

All pointers are i32 indices into this memory.

### Function Example

```whist
func add(a: i32, b: i32): i32 {
    return a + b;
}
```

```wasm
(func $add (param $a i32) (param $b i32) (result i32)
    local.get $a
    local.get $b
    i32.add
)
```

### Calling Conventions

WASM uses stack-based execution:

```whist
func example(): i32 {
    return add(1, 2) * 3;
}
```

```wasm
(func $example (result i32)
    i32.const 1      ;; push 1
    i32.const 2      ;; push 2
    call $add        ;; pop 2 args, push result
    i32.const 3      ;; push 3
    i32.mul          ;; pop 2, push product
)
```

## Structs in WASM

Structs are laid out in linear memory:

```whist
struct Point { x: i32, y: i32 }

func make_point(): Point {
    return Point { x: 10, y: 20 };
}
```

```wasm
;; Point is 8 bytes: x at offset 0, y at offset 4
(func $make_point (result i32)  ;; returns pointer
    (local $ptr i32)

    ;; Allocate 8 bytes
    i32.const 8
    call $alloc
    local.set $ptr

    ;; Store x
    local.get $ptr
    i32.const 10
    i32.store

    ;; Store y
    local.get $ptr
    i32.const 4
    i32.add
    i32.const 20
    i32.store

    local.get $ptr
)
```

## String Handling

Strings need special handling for WASM:

### Option 1: Copy to/from JavaScript

```javascript
// JavaScript side
function copyStringToWasm(str) {
    const bytes = new TextEncoder().encode(str);
    const ptr = wasmExports.alloc(bytes.length + 1);
    const memory = new Uint8Array(wasmExports.memory.buffer);
    memory.set(bytes, ptr);
    memory[ptr + bytes.length] = 0;  // null terminate
    return ptr;
}
```

### Option 2: String References

Use externref (WASM reference types):

```wasm
(func $print (param $str externref)
    local.get $str
    call $js_print  ;; imported JS function
)
```

## JavaScript Interop

### Importing JS Functions

```wasm
(import "env" "console_log" (func $console_log (param i32 i32)))
```

```javascript
const imports = {
    env: {
        console_log: (ptr, len) => {
            const bytes = new Uint8Array(memory.buffer, ptr, len);
            console.log(new TextDecoder().decode(bytes));
        }
    }
};
```

### Exporting WASM Functions

```wasm
(export "add" (func $add))
(export "memory" (memory 0))
```

```javascript
const result = wasmExports.add(1, 2);
```

## Standard Library for WASM

Minimal runtime needed:

```whist
// Memory management
extern func alloc(size: i32): i32;
extern func free(ptr: i32): void;

// Console output
extern func print(s: string): void;
extern func print_i32(n: i32): void;

// Math (can use WASM builtins)
extern func sqrt(x: f64): f64;
extern func sin(x: f64): f64;

// Time
extern func now(): f64;  // milliseconds

// Random
extern func random(): f64;  // 0.0 to 1.0
```

## WASI Support

WebAssembly System Interface for non-browser environments:

```whist
// File I/O via WASI
extern func fd_read(fd: i32, iovs: i32, iovs_len: i32, nread: i32): i32;
extern func fd_write(fd: i32, iovs: i32, iovs_len: i32, nwritten: i32): i32;

// Args and environment
extern func args_get(argv: i32, argv_buf: i32): i32;
extern func environ_get(environ: i32, environ_buf: i32): i32;
```

Run WASI modules with:
```bash
wasmtime program.wasm
wasmer program.wasm
wasm3 program.wasm
```

## Build Modes

### Browser Bundle

Generate .wasm + loader .js:

```bash
wc program.w --target=wasm32 --bundle -o program.js
```

Output:
```
program.wasm    # WebAssembly binary
program.js      # JavaScript loader/glue
```

Usage:
```html
<script src="program.js"></script>
<script>
    WhistProgram.init().then(mod => {
        mod.main();
    });
</script>
```

### WASI Executable

Standalone WASM module:

```bash
wc program.w --target=wasm32-wasi -o program.wasm
wasmtime program.wasm
```

### Library

Export functions for use from JavaScript:

```bash
wc lib.w --target=wasm32 --lib -o lib.wasm
```

```javascript
const { add, multiply } = await loadWasm('lib.wasm');
console.log(add(1, 2));
```

## Implementation Phases

### Phase 1: Basic Compilation

1. Emit WASM text format (.wat)
2. Use wat2wasm to produce .wasm
3. Basic functions and arithmetic
4. Simple control flow

### Phase 2: Full Language

1. Struct layout and access
2. Arrays and memory management
3. Methods (as regular functions)
4. Generics (monomorphize first)

### Phase 3: JavaScript Interop

1. Export functions
2. Import JavaScript functions
3. String passing
4. Memory sharing

### Phase 4: Polish

1. Binary WASM generation (skip wat2wasm)
2. Optimizations
3. Debug info (DWARF in WASM)
4. Source maps

## Challenges

### 1. Garbage Collection

WASM doesn't have GC (yet):
- Manual memory management
- Reference counting
- Or use WasmGC proposal when stable

### 2. No Direct DOM Access

Must go through JavaScript:
```whist
// Can't do this directly
document.getElementById("foo");

// Must import from JS
extern func get_element(id: string): Element;
```

### 3. 32-bit Pointers

WASM32 uses 32-bit addresses:
- Maximum 4GB memory
- i64 for data, i32 for pointers
- WASM64 coming eventually

### 4. Async/Await

WASM is synchronous:
- Can't await promises directly
- Need Asyncify or stack switching
- Or design around it

## Open Questions

1. **Via LLVM or direct?**
   - LLVM is easier, less control
   - Direct gives smaller output

2. **GC strategy?**
   - Manual (current approach)
   - Reference counting
   - Wait for WasmGC

3. **String representation?**
   - UTF-8 in linear memory
   - JavaScript string references
   - Hybrid approach

4. **Browser vs WASI focus?**
   - Browser needs JS glue
   - WASI is more portable
   - Support both?

5. **Bundle format?**
   - Separate .wasm + .js
   - Single .js with embedded WASM
   - HTML template

## Example: Complete Web App

```whist
// game.w
import wasm;

struct Game {
    x: f32,
    y: f32,
    score: i32,
}

public func init(): *Game {
    var game = wasm.alloc::<Game>();
    game.x = 400.0;
    game.y = 300.0;
    game.score = 0;
    return game;
}

public func update(game: *Game, dt: f32): void {
    // Update game state
}

public func render(game: *Game): void {
    wasm.clear_canvas();
    wasm.draw_rect(game.x, game.y, 50.0, 50.0);
    wasm.draw_text(10.0, 10.0, "Score: {game.score}");
}
```

```javascript
// game.js
const game = await WhistGame.init();

function gameLoop(timestamp) {
    const dt = timestamp - lastTime;
    WhistGame.update(game, dt);
    WhistGame.render(game);
    requestAnimationFrame(gameLoop);
}
requestAnimationFrame(gameLoop);
```

## Related Features

- [LLVM Backend](llvm-backend.md) - WASM via LLVM
- [Memory Management](memory-management.md) - Manual memory for WASM
- [Self-Hosting](self-hosting.md) - Could compile wc to WASM
