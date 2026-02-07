# Incremental Compilation

Only recompile what changed for faster iteration.

## Goals

1. **Fast rebuilds** - Seconds, not minutes, for large projects
2. **Accurate dependencies** - Never miss a change, never over-rebuild
3. **Cached artifacts** - Persist work across compilations
4. **Parallel builds** - Utilize multiple cores

## Current State

Full recompilation every time:
```
w0 program.w → parse all → check all → generate all → C compile all
```

Even small changes rebuild everything.

## Target State

Incremental pipeline:
```
Change file.w
  → Detect affected modules
  → Re-parse only file.w
  → Re-check affected modules
  → Re-generate affected C files
  → C compile only changed .c files
  → Link
```

## Dependency Tracking

### Module Dependencies

Track which modules import which:

```
main.w imports utils.w, http.w
http.w imports utils.w, socket.w
utils.w imports std
```

Dependency graph:
```
main.w ──→ utils.w ──→ std
   │          ↑
   └──→ http.w ┘
           │
           └──→ socket.w ──→ std
```

### Symbol Dependencies

Finer-grained: track which symbols are used:

```whist
// main.w
import utils;

func main(): i32 {
    utils.log("hello");  // depends on utils.log
    return 0;
}
```

If `utils.format` changes but `utils.log` doesn't, no rebuild needed.

### Type Dependencies

Track type definitions used:

```whist
// main.w
import types;

func process(p: types.Point): void {  // depends on types.Point layout
    print(p.x);
}
```

If `Point` fields change, must rebuild `process`.

## Build Cache

### Cache Structure

```
.whist-cache/
├── manifest.json           # Cache metadata
├── deps/
│   └── main.w.deps         # Dependency info per file
├── ast/
│   ├── main.w.ast          # Cached AST
│   └── utils.w.ast
├── types/
│   ├── main.w.types        # Type check results
│   └── utils.w.types
├── codegen/
│   ├── main.c              # Generated C
│   └── utils.c
└── obj/
    ├── main.o              # Compiled objects
    └── utils.o
```

### Cache Invalidation

Invalidate based on:

1. **File content hash** - Source file changed
2. **Compiler version** - Compiler updated
3. **Compiler flags** - Build options changed
4. **Dependencies changed** - Imported module changed

```whist
struct CacheEntry {
    source_hash: u64,
    compiler_version: string,
    flags_hash: u64,
    dependency_hashes: HashMap<string, u64>,
    artifact_path: string,
}

func is_valid(entry: CacheEntry, file: string): bool {
    if hash_file(file) != entry.source_hash {
        return false;
    }
    if COMPILER_VERSION != entry.compiler_version {
        return false;
    }
    foreach (dep, hash) in entry.dependency_hashes {
        if !is_valid(cache.get(dep), dep) {
            return false;
        }
    }
    return true;
}
```

## Compilation Phases

### Phase 1: Discover Changes

```whist
func discover_changes(project: Project): Vec<string> {
    var changed = Vec::new();

    foreach file in project.source_files {
        var entry = cache.get(file);
        if entry == null || !is_valid(entry, file) {
            changed.push(file);
        }
    }

    return changed;
}
```

### Phase 2: Compute Affected Set

```whist
func affected_modules(changed: Vec<string>, deps: DependencyGraph): Vec<string> {
    var affected = HashSet::from(changed);
    var worklist = changed.clone();

    while !worklist.is_empty() {
        var file = worklist.pop();
        // Find modules that depend on this one
        foreach dependent in deps.dependents(file) {
            if !affected.contains(dependent) {
                affected.insert(dependent);
                worklist.push(dependent);
            }
        }
    }

    return affected.to_vec();
}
```

### Phase 3: Incremental Parse

```whist
func incremental_parse(affected: Vec<string>): HashMap<string, Ast> {
    var asts = HashMap::new();

    foreach file in affected {
        var ast = parse(read_file(file));
        asts.insert(file, ast);
        cache.store_ast(file, ast);
    }

    // Load cached ASTs for unchanged files
    foreach file in project.source_files {
        if !affected.contains(file) {
            asts.insert(file, cache.load_ast(file));
        }
    }

    return asts;
}
```

### Phase 4: Incremental Type Check

```whist
func incremental_check(affected: Vec<string>, asts: HashMap<string, Ast>): CheckResult {
    // Type check in dependency order
    var order = topological_sort(affected, deps);

    foreach file in order {
        var result = check(asts.get(file), context);
        cache.store_types(file, result);
    }

    return collect_results();
}
```

### Phase 5: Incremental Codegen

```whist
func incremental_codegen(affected: Vec<string>, checked: CheckResult): void {
    foreach file in affected {
        var c_code = generate(checked.get(file));
        write_file(cache_path(file, ".c"), c_code);
    }
}
```

### Phase 6: Compile and Link

```whist
func compile_and_link(affected: Vec<string>): void {
    // Compile changed C files in parallel
    parallel_for(affected, |file| {
        var c_file = cache_path(file, ".c");
        var o_file = cache_path(file, ".o");
        exec("cc", ["-c", c_file, "-o", o_file]);
    });

    // Link all object files
    var objects = project.source_files.map(|f| cache_path(f, ".o"));
    exec("cc", objects + ["-o", output_path]);
}
```

## Parallel Compilation

### Dependency-Aware Parallelism

Compile independent modules in parallel:

```
     main.w ←─────────────┐
       ↓                  │
    http.w ←───┐          │
       ↓       │          │
   socket.w   utils.w     │
       ↓       ↓          │
      std.w ──────────────┘
```

Can compile in parallel:
- Level 0: std.w
- Level 1: socket.w, utils.w (parallel)
- Level 2: http.w
- Level 3: main.w

```whist
func parallel_compile(deps: DependencyGraph): void {
    var levels = compute_levels(deps);

    foreach level in levels {
        parallel_for(level, |file| {
            compile_module(file);
        });
        // Barrier: wait for level to complete
    }
}
```

### Work Stealing

For better load balancing:

```whist
func compile_with_work_stealing(files: Vec<string>): void {
    var queue = ConcurrentQueue::from(files);
    var workers = spawn_workers(num_cpus());

    foreach worker in workers {
        spawn {
            while let Some(file) = queue.pop() {
                if dependencies_ready(file) {
                    compile_module(file);
                } else {
                    queue.push(file);  // Not ready, put back
                }
            }
        };
    }

    join_all(workers);
}
```

## Watch Mode

Continuous compilation on file changes:

```bash
wc --watch src/
```

```whist
func watch_mode(project: Project): void {
    // Initial full build
    full_build(project);

    // Watch for changes
    var watcher = FileWatcher::new(project.source_dirs);

    foreach event in watcher.events() {
        match event {
            FileChanged(path) => {
                var affected = affected_modules([path], deps);
                incremental_build(affected);
            },
            FileCreated(path) => {
                add_to_project(path);
                incremental_build([path]);
            },
            FileDeleted(path) => {
                remove_from_project(path);
                full_rebuild();  // Safer for deletions
            },
        }
    }
}
```

## Interface Stability

Avoid rebuilding dependents when only implementation changes:

### Module Interface

```whist
// utils.w
public func format(s: string): string {  // Interface: signature
    // Implementation can change without affecting dependents
    return "[" + s + "]";
}
```

### Interface Hash

Hash only public signatures:

```whist
struct ModuleInterface {
    public_functions: Vec<FunctionSignature>,
    public_types: Vec<TypeDefinition>,
    public_constants: Vec<(string, Type)>,
}

func interface_hash(module: Module): u64 {
    var iface = extract_interface(module);
    return hash(iface);
}
```

Only rebuild dependents if interface hash changes:

```whist
func needs_dependent_rebuild(file: string): bool {
    var old_hash = cache.get_interface_hash(file);
    var new_hash = interface_hash(parse(file));
    return old_hash != new_hash;
}
```

## Challenges

### 1. Macro Expansion

If Whist adds macros, they complicate dependencies:
- Macro definition change affects all uses
- Macro can generate imports
- May need to re-expand before dependency analysis

### 2. Generic Instantiation

Generics are monomorphized:
- `Vec<i32>` and `Vec<string>` are separate
- Change to `Vec<T>` affects all instantiations
- Track instantiation sites

### 3. Global Type Inference

Type inference can propagate:
- Change in one function affects inferred types elsewhere
- May need conservative over-rebuild
- Or track inference dependencies

### 4. Build Reproducibility

Same inputs should produce same outputs:
- Deterministic ordering
- No timestamps in output
- Consistent hashing

## Open Questions

1. **Cache location?**
   - Project-local (`.whist-cache/`)
   - User-global (`~/.cache/whist/`)
   - Configurable

2. **Cache format?**
   - Binary (fast, version-sensitive)
   - JSON (debuggable, slower)
   - SQLite (queryable, atomic)

3. **Granularity?**
   - File-level (simpler)
   - Function-level (more precise)
   - Expression-level (very complex)

4. **Distributed cache?**
   - Share cache across team
   - Like ccache or sccache
   - Cloud-based

5. **Integration with LSP?**
   - Share incremental infrastructure
   - LSP needs even faster updates

## Metrics

Track to measure effectiveness:

- Cache hit rate
- Rebuild time (full vs incremental)
- Number of files recompiled
- Parallelism efficiency

```bash
wc --stats build
# Files: 100
# Changed: 3
# Rebuilt: 7 (due to dependencies)
# Cache hits: 93
# Build time: 0.8s (vs 12s full)
```

## Examples

### Small Change

```
Edit: utils.w (fix typo in implementation)

Affected: utils.w only (interface unchanged)
Actions:
  - Parse utils.w
  - Check utils.w
  - Generate utils.c
  - Compile utils.o
  - Link

Time: 0.2s
```

### Interface Change

```
Edit: utils.w (change function signature)

Affected: utils.w, main.w, http.w (all users)
Actions:
  - Parse utils.w, main.w, http.w
  - Check all three
  - Generate all three .c files
  - Compile all three .o files
  - Link

Time: 0.5s
```

### New File

```
Add: new_module.w

Affected: new_module.w, files that import it
Actions:
  - Update dependency graph
  - Parse new_module.w
  - Check new_module.w
  - Generate new_module.c
  - Compile new_module.o
  - Link

Time: 0.3s
```

## Related Features

- [LSP Server](lsp-server.md) - Shares incremental analysis
- [Self-Hosting](self-hosting.md) - Incremental for faster compiler dev
- [LLVM Backend](llvm-backend.md) - Cache LLVM modules
