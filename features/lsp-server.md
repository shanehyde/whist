# LSP Server

Language Server Protocol implementation for IDE integration.

## Goals

1. **IDE integration** - VS Code, Neovim, Emacs, etc.
2. **Real-time feedback** - Errors as you type
3. **Code intelligence** - Go to definition, find references, etc.
4. **Developer productivity** - Auto-complete, hover info, refactoring

## LSP Features

### Core Features (Priority 1)

| Feature | Description |
|---------|-------------|
| **Diagnostics** | Syntax errors, type errors, warnings |
| **Hover** | Type info and documentation on hover |
| **Go to Definition** | Jump to where symbol is defined |
| **Find References** | Find all uses of a symbol |
| **Document Symbols** | Outline view of file |
| **Completion** | Auto-complete suggestions |

### Extended Features (Priority 2)

| Feature | Description |
|---------|-------------|
| **Rename** | Rename symbol across files |
| **Code Actions** | Quick fixes and refactorings |
| **Formatting** | Auto-format code |
| **Signature Help** | Function parameter hints |
| **Workspace Symbols** | Search symbols across project |
| **Folding Ranges** | Code folding regions |

### Advanced Features (Priority 3)

| Feature | Description |
|---------|-------------|
| **Semantic Tokens** | Rich syntax highlighting |
| **Inlay Hints** | Inline type annotations |
| **Call Hierarchy** | Incoming/outgoing calls |
| **Type Hierarchy** | Super/subtypes |
| **Code Lens** | Inline actionable info |

## Architecture

```
┌─────────────────┐     JSON-RPC      ┌─────────────────┐
│                 │ ←───────────────→ │                 │
│   IDE/Editor    │                   │   whist-lsp     │
│   (VS Code)     │                   │   (Whist LSP)   │
│                 │                   │                 │
└─────────────────┘                   └─────────────────┘
                                              │
                                              ▼
                                      ┌───────────────┐
                                      │   Compiler    │
                                      │   Frontend    │
                                      │ (lexer/parser │
                                      │  /checker)    │
                                      └───────────────┘
```

### Components

```
whist-lsp/
├── main.w              # Entry point, stdio transport
├── server.w            # LSP server state and handlers
├── protocol.w          # LSP message types
├── transport.w         # JSON-RPC over stdio
├── analysis/
│   ├── index.w         # Symbol indexing
│   ├── hover.w         # Hover information
│   ├── completion.w    # Auto-completion
│   ├── references.w    # Find references
│   └── diagnostics.w   # Error reporting
└── utils/
    ├── position.w      # Line/column utilities
    └── uri.w           # File URI handling
```

## LSP Protocol

### Initialization

```json
// Client → Server
{
    "jsonrpc": "2.0",
    "id": 1,
    "method": "initialize",
    "params": {
        "rootUri": "file:///path/to/project",
        "capabilities": { ... }
    }
}

// Server → Client
{
    "jsonrpc": "2.0",
    "id": 1,
    "result": {
        "capabilities": {
            "textDocumentSync": 1,
            "hoverProvider": true,
            "completionProvider": { "triggerCharacters": [".", ":"] },
            "definitionProvider": true,
            "referencesProvider": true
        }
    }
}
```

### Document Sync

```json
// Client → Server: File opened
{
    "method": "textDocument/didOpen",
    "params": {
        "textDocument": {
            "uri": "file:///path/to/file.w",
            "text": "func main() -> i32 { ... }"
        }
    }
}

// Client → Server: File changed
{
    "method": "textDocument/didChange",
    "params": {
        "textDocument": { "uri": "file:///path/to/file.w" },
        "contentChanges": [{ "text": "new content..." }]
    }
}
```

### Diagnostics

```json
// Server → Client: Publish diagnostics
{
    "method": "textDocument/publishDiagnostics",
    "params": {
        "uri": "file:///path/to/file.w",
        "diagnostics": [
            {
                "range": { "start": {"line": 5, "character": 10}, "end": {...} },
                "severity": 1,  // Error
                "message": "Type mismatch: expected i32, got string"
            }
        ]
    }
}
```

### Hover

```json
// Client → Server
{
    "method": "textDocument/hover",
    "params": {
        "textDocument": { "uri": "file:///path/to/file.w" },
        "position": { "line": 10, "character": 15 }
    }
}

// Server → Client
{
    "result": {
        "contents": {
            "kind": "markdown",
            "value": "```whist\nfunc add(a: i64, b: i64) -> i64\n```\nAdds two numbers."
        }
    }
}
```

### Completion

```json
// Client → Server
{
    "method": "textDocument/completion",
    "params": {
        "textDocument": { "uri": "file:///path/to/file.w" },
        "position": { "line": 10, "character": 5 }
    }
}

// Server → Client
{
    "result": [
        { "label": "print", "kind": 3, "detail": "func(string) -> void" },
        { "label": "println", "kind": 3, "detail": "func(string) -> void" },
        { "label": "Point", "kind": 22, "detail": "struct" }
    ]
}
```

## Server State

```whist
struct LspServer {
    // Document state
    documents: HashMap<string, Document>,

    // Project state
    root_uri: string,
    index: SymbolIndex,

    // Compiler integration
    // (reuse compiler frontend)
}

struct Document {
    uri: string,
    content: string,
    version: i32,
    ast: ?Module,           // Cached parse result
    diagnostics: Vec<Diagnostic>,
}

struct SymbolIndex {
    // Symbol → Definition location
    definitions: HashMap<string, Location>,

    // Location → Symbol references
    references: HashMap<Location, Vec<Location>>,

    // Scope tree for completion
    scopes: Vec<Scope>,
}
```

## Incremental Analysis

For responsive editing:

### 1. Incremental Parsing

Re-parse only changed regions:

```whist
func on_change(doc: *Document, changes: Vec<Change>) -> void {
    // For small changes, patch the AST
    // For large changes, full re-parse
    if changes.len == 1 && changes[0].is_small() {
        patch_ast(doc, changes[0]);
    } else {
        doc.ast = parse(doc.content);
    }

    // Re-check types
    check(doc);
}
```

### 2. Debouncing

Don't analyze on every keystroke:

```whist
func on_change(doc: *Document) -> void {
    cancel_pending_analysis(doc);
    schedule_analysis(doc, delay: 100);  // 100ms debounce
}
```

### 3. Background Analysis

Analyze in separate thread:

```whist
func analyze_async(doc: *Document) -> void {
    spawn {
        var result = full_analysis(doc);
        send_diagnostics(doc.uri, result.diagnostics);
    };
}
```

## Compiler Integration

LSP reuses compiler frontend:

```whist
// Shared with compiler
import lexer;
import parser;
import checker;
import ast;
import types;

func analyze(content: string) -> AnalysisResult {
    var tokens = lexer.tokenize(content);
    var ast = parser.parse(tokens);
    var (typed_ast, errors) = checker.check(ast);

    return AnalysisResult {
        ast: typed_ast,
        errors: errors,
        symbols: extract_symbols(typed_ast),
    };
}
```

## VS Code Extension

Basic extension structure:

```
whist-vscode/
├── package.json        # Extension manifest
├── src/
│   └── extension.ts    # Extension entry point
└── syntaxes/
    └── whist.tmLanguage.json  # TextMate grammar
```

```json
// package.json
{
    "name": "whist",
    "displayName": "Whist Language",
    "description": "Whist language support",
    "main": "./out/extension.js",
    "contributes": {
        "languages": [{
            "id": "whist",
            "extensions": [".w"],
            "configuration": "./language-configuration.json"
        }],
        "grammars": [{
            "language": "whist",
            "scopeName": "source.whist",
            "path": "./syntaxes/whist.tmLanguage.json"
        }]
    }
}
```

```typescript
// extension.ts
import * as vscode from 'vscode';
import { LanguageClient } from 'vscode-languageclient/node';

export function activate(context: vscode.ExtensionContext) {
    const serverPath = context.asAbsolutePath('whist-lsp');

    const client = new LanguageClient(
        'whist',
        'Whist Language Server',
        { command: serverPath },
        { documentSelector: [{ scheme: 'file', language: 'whist' }] }
    );

    client.start();
}
```

## Implementation Phases

### Phase 1: Basic Server

1. JSON-RPC transport over stdio
2. Document synchronization
3. Diagnostics (parse/type errors)
4. Basic hover (show type)

### Phase 2: Navigation

1. Go to definition
2. Find references
3. Document symbols
4. Workspace symbols

### Phase 3: Editing Support

1. Completion (keywords, symbols)
2. Signature help
3. Rename symbol
4. Formatting

### Phase 4: Advanced

1. Semantic highlighting
2. Inlay hints (type annotations)
3. Code actions (quick fixes)
4. Refactoring support

## Challenges

### 1. Incremental Re-analysis

Need to be fast on every keystroke:
- Cache parse trees
- Incremental type checking
- Debounce rapid changes

### 2. Error Recovery

Parse incomplete/invalid code:
- Synchronization points
- Partial ASTs
- Best-effort analysis

### 3. Multi-file Projects

Track dependencies:
- File watcher for changes
- Incremental re-indexing
- Import resolution

### 4. Performance

Stay responsive:
- Background processing
- Cancellation of outdated requests
- Memory management

## Open Questions

1. **Language for LSP server?**
   - Whist (dogfooding, but chicken-egg)
   - C (reuse w0 code)
   - TypeScript (LSP libraries available)

2. **Standalone or integrated?**
   - Separate binary (`whist-lsp`)
   - Flag on compiler (`wc --lsp`)

3. **Protocol transport?**
   - stdio (standard)
   - TCP socket (debugging)
   - Node IPC (VS Code)

4. **Multi-root workspaces?**
   - Multiple project roots
   - Separate analysis contexts

## Example Session

```
Editor opens file.w
  → textDocument/didOpen

User types "std."
  → textDocument/didChange
  → textDocument/completion
  ← [print, abs_i64, max_i64, ...]

User hovers over "print"
  → textDocument/hover
  ← "func print(s: string) -> void"

User Ctrl+clicks "Point"
  → textDocument/definition
  ← Location { uri: "types.w", line: 15 }

User renames "foo" to "bar"
  → textDocument/rename
  ← WorkspaceEdit { changes across files }
```

## Related Features

- [Self-Hosting](self-hosting.md) - LSP written in Whist
- [Incremental Compilation](incremental-compilation.md) - Shared incremental infrastructure
