# Package Manager

Dependency management for Whist projects.

## Goals

1. **Dependency management** - Declare and resolve project dependencies
2. **Version resolution** - Handle version constraints and conflicts
3. **Reproducible builds** - Lock files ensure consistent builds
4. **Package registry** - Central repository for sharing packages
5. **Build integration** - Seamlessly integrate with compiler

## Command Line Interface

```bash
# Project management
whist init                    # Create new project
whist init --lib              # Create library project
whist build                   # Build project
whist run                     # Build and run
whist test                    # Run tests
whist check                   # Type check only

# Dependency management
whist add <package>           # Add dependency
whist add <package>@<version> # Add specific version
whist remove <package>        # Remove dependency
whist update                  # Update dependencies
whist update <package>        # Update specific package

# Publishing
whist login                   # Authenticate with registry
whist publish                 # Publish package
whist yank <version>          # Yank a version (discourage use)

# Utilities
whist search <query>          # Search registry
whist info <package>          # Show package info
whist doc                     # Generate documentation
whist fmt                     # Format code
whist clean                   # Clean build artifacts
```

## Project Structure

```
my-project/
├── whist.toml              # Project manifest
├── whist.lock              # Lock file (generated)
├── src/
│   ├── main.w              # Entry point (binary)
│   └── lib.w               # Entry point (library)
├── tests/
│   └── test_*.w            # Test files
├── examples/
│   └── example.w           # Example programs
├── benches/
│   └── bench_*.w           # Benchmarks
└── .whist/
    └── cache/              # Local build cache
```

## Project Manifest (whist.toml)

```toml
[package]
name = "my-project"
version = "0.1.0"
authors = ["Alice <alice@example.com>"]
description = "A cool Whist project"
license = "MIT"
repository = "https://github.com/alice/my-project"
keywords = ["cli", "utility"]
categories = ["command-line-utilities"]

[dependencies]
http = "1.2.3"
json = { version = "2.0", features = ["serde"] }
utils = { git = "https://github.com/bob/utils", branch = "main" }
local-lib = { path = "../local-lib" }

[dev-dependencies]
test-utils = "0.5"
benchmark = "1.0"

[build-dependencies]
codegen = "0.2"

[features]
default = ["std"]
std = []
async = ["dep:async-runtime"]
full = ["std", "async", "logging"]

[[bin]]
name = "my-cli"
path = "src/main.w"

[[lib]]
name = "my-lib"
path = "src/lib.w"

[profile.release]
opt-level = 3
debug = false

[profile.dev]
opt-level = 0
debug = true
```

## Lock File (whist.lock)

```toml
# Auto-generated, do not edit
version = 1

[[package]]
name = "http"
version = "1.2.3"
source = "registry"
checksum = "sha256:abc123..."
dependencies = ["url", "socket"]

[[package]]
name = "url"
version = "2.1.0"
source = "registry"
checksum = "sha256:def456..."

[[package]]
name = "utils"
version = "0.0.0"
source = "git+https://github.com/bob/utils?branch=main#a1b2c3d4"
```

## Version Resolution

### Semantic Versioning

```toml
[dependencies]
# Exact version
exact = "=1.2.3"

# Caret (default): compatible updates
caret = "^1.2.3"    # >=1.2.3, <2.0.0
caret2 = "1.2.3"    # same as ^1.2.3

# Tilde: patch updates only
tilde = "~1.2.3"    # >=1.2.3, <1.3.0

# Wildcard
wild = "1.2.*"      # >=1.2.0, <1.3.0
wild2 = "1.*"       # >=1.0.0, <2.0.0

# Range
range = ">=1.2.3, <2.0.0"
```

### Resolution Algorithm

```whist
func resolve(dependencies: Vec<Dependency>): Result<Solution, Error> {
    // 1. Build dependency graph
    var graph = build_graph(dependencies);

    // 2. Topological sort
    var order = topological_sort(graph)?;

    // 3. For each package, find compatible version
    var solution = HashMap::new();

    foreach pkg in order {
        var constraints = collect_constraints(pkg, solution);
        var version = find_best_version(pkg, constraints)?;
        solution.insert(pkg, version);
    }

    return Ok(solution);
}

func find_best_version(pkg: string, constraints: Vec<Constraint>): Result<Version, Error> {
    var versions = registry.get_versions(pkg)?;

    // Filter by constraints
    var compatible = versions.filter(|v| constraints.all(|c| c.matches(v)));

    // Return highest compatible version
    compatible.max().ok_or(Error::NoCompatibleVersion(pkg))
}
```

### Conflict Resolution

```
Project depends on:
  - A ^1.0 (requires C ^1.0)
  - B ^1.0 (requires C ^2.0)

Error: Version conflict for C
  - A 1.2.3 requires C ^1.0
  - B 1.0.5 requires C ^2.0

Possible solutions:
  1. Upgrade A to version that supports C ^2.0
  2. Downgrade B to version that supports C ^1.0
  3. Use different versions of C (if supported)
```

## Package Registry

### Registry API

```
GET  /api/v1/packages                    # List packages
GET  /api/v1/packages/{name}             # Package info
GET  /api/v1/packages/{name}/{version}   # Version info
GET  /api/v1/packages/{name}/{version}/download  # Download
POST /api/v1/packages                    # Publish (authenticated)
DELETE /api/v1/packages/{name}/{version} # Yank (authenticated)
GET  /api/v1/search?q={query}            # Search
```

### Package Metadata

```json
{
    "name": "http",
    "description": "HTTP client and server",
    "versions": [
        {
            "version": "1.2.3",
            "published": "2024-01-15T10:30:00Z",
            "checksum": "sha256:abc123...",
            "dependencies": {
                "url": "^2.0",
                "socket": "^1.5"
            },
            "features": {
                "default": ["client"],
                "client": [],
                "server": ["dep:async"],
                "full": ["client", "server"]
            },
            "yanked": false
        }
    ],
    "owners": ["alice", "bob"],
    "repository": "https://github.com/whist-lang/http",
    "documentation": "https://docs.whist-lang.org/http",
    "downloads": 12345
}
```

## Dependency Sources

### Registry (Default)

```toml
http = "1.2.3"
http = { version = "1.2.3", registry = "https://packages.whist-lang.org" }
```

### Git Repository

```toml
# Branch
utils = { git = "https://github.com/user/utils", branch = "main" }

# Tag
utils = { git = "https://github.com/user/utils", tag = "v1.0.0" }

# Commit
utils = { git = "https://github.com/user/utils", rev = "a1b2c3d" }
```

### Local Path

```toml
local-lib = { path = "../local-lib" }
```

### Private Registry

```toml
# In whist.toml
[registries]
company = "https://packages.company.com"

[dependencies]
internal-lib = { version = "1.0", registry = "company" }
```

## Features

Conditional compilation and optional dependencies:

```toml
[features]
default = ["std"]
std = []
async = ["dep:async-runtime"]
logging = ["dep:log", "std"]
full = ["std", "async", "logging"]

[dependencies]
async-runtime = { version = "1.0", optional = true }
log = { version = "0.4", optional = true }
```

```whist
#[cfg(feature = "async")]
import async_runtime;

#[cfg(feature = "logging")]
func log(msg: string): void {
    log.info(msg);
}

#[cfg(not(feature = "logging"))]
func log(msg: string): void {
    // no-op
}
```

## Build Scripts

Custom build logic:

```toml
[package]
build = "build.w"

[build-dependencies]
codegen = "0.2"
```

```whist
// build.w
import codegen;
import std.env;
import std.fs;

func main(): i32 {
    // Generate code
    var generated = codegen.generate_bindings("lib.h");
    fs.write("src/generated.w", generated)?;

    // Set compile-time variables
    env.set_var("WHIST_BUILD_VERSION", "1.2.3");

    return 0;
}
```

## Workspaces

Multi-package projects:

```
my-workspace/
├── whist.toml              # Workspace manifest
├── packages/
│   ├── core/
│   │   ├── whist.toml
│   │   └── src/
│   ├── cli/
│   │   ├── whist.toml
│   │   └── src/
│   └── web/
│       ├── whist.toml
│       └── src/
└── shared/
    └── whist.toml
```

```toml
# Root whist.toml
[workspace]
members = [
    "packages/*",
    "shared",
]

[workspace.dependencies]
serde = "1.0"
log = "0.4"
```

```toml
# packages/cli/whist.toml
[package]
name = "my-cli"
version = "0.1.0"

[dependencies]
core = { path = "../core" }
serde = { workspace = true }
```

## Implementation Phases

### Phase 1: Basic Package Management

1. Project manifest parsing
2. Local path dependencies
3. Build command
4. Run command

### Phase 2: Version Resolution

1. Semantic versioning
2. Lock file generation
3. Dependency resolution
4. Update command

### Phase 3: Registry

1. Registry API client
2. Package download/cache
3. Search command
4. Publish command

### Phase 4: Advanced Features

1. Features and optional deps
2. Workspaces
3. Build scripts
4. Private registries

## Challenges

### 1. Diamond Dependencies

```
    A
   / \
  B   C
   \ /
    D (different versions?)
```

Solutions:
- Require single version (simpler)
- Allow multiple versions (complex, larger binary)

### 2. Cyclic Dependencies

```
A depends on B
B depends on A
```

Prevent cycles in dependency graph.

### 3. Reproducibility

Ensure same lock file produces same build:
- Pin all transitive dependencies
- Include checksums
- Avoid floating versions in lock

### 4. Security

- Verify package checksums
- Sign packages
- Audit dependencies for vulnerabilities
- Prevent typosquatting

## Open Questions

1. **Package naming?**
   - Flat namespace (`http`)
   - Scoped (`@user/http`)

2. **Default registry?**
   - Self-hosted initially
   - Community-run eventually

3. **Binary distribution?**
   - Source-only packages
   - Pre-built binaries for speed

4. **Vendoring?**
   - Copy dependencies into project
   - For offline builds / auditing

5. **Multiple versions?**
   - One version per package (simpler)
   - Multiple versions allowed (flexible)

## Examples

### Creating a New Project

```bash
$ whist init my-app
Created project 'my-app'

$ cd my-app
$ tree
.
├── whist.toml
└── src
    └── main.w

$ cat whist.toml
[package]
name = "my-app"
version = "0.1.0"

[dependencies]

$ cat src/main.w
import std;

func main(): i32 {
    std.print("Hello, World!\n");
    return 0;
}
```

### Adding Dependencies

```bash
$ whist add http json

Added http v1.2.3
Added json v2.0.1

$ cat whist.toml
[package]
name = "my-app"
version = "0.1.0"

[dependencies]
http = "1.2.3"
json = "2.0.1"
```

### Building and Running

```bash
$ whist build
   Compiling http v1.2.3
   Compiling json v2.0.1
   Compiling my-app v0.1.0
    Finished dev [unoptimized + debuginfo] in 1.23s

$ whist run
Hello, World!

$ whist run --release
    Finished release [optimized] in 2.45s
Hello, World!
```

## Related Features

- [Incremental Compilation](incremental-compilation.md) - Cached builds
- [Self-Hosting](self-hosting.md) - Package manager in Whist
