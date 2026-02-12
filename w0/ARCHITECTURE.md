# w0 Architecture Contracts

This document defines maintainability-oriented contracts for the bootstrap compiler.
It describes how compilation phases interact, what each phase owns, and where
future refactors should push the codebase.

## Pipeline Overview

The current pipeline is:

1. Lexer (`lexer.c`): token stream from source text.
2. Parser (`parser.c`): syntax AST (`Node`) construction and import expansion via `ModuleLoader`.
3. Checker (`checker*.c`): symbol resolution, type checking, generic instantiation, semantic annotation.
4. Codegen (`codegen*.c`): C emission from typed AST + checker output snapshot.
5. Driver (`main.c`): CLI orchestration, temporary-file run mode, C compiler invocation.

## Phase Ownership

### Parser Owns Syntax

- Responsible for producing syntactically valid AST shape.
- Must not perform semantic resolution requiring symbol/type context.
- `Node` fields representing source syntax are parser-owned and should be stable after parse.

### Checker Owns Semantics

- Responsible for types, symbol visibility, trait/impl validation, and generic instantiation.
- May attach semantic information required by downstream phases.
- Should avoid rewriting parser-owned syntax data. Existing legacy mutations should not be expanded.

### Codegen Is a Consumer

- Codegen should treat AST syntax as read-only.
- Codegen may consume checker-produced semantic data.
- New backend-only hints should prefer sidecar metadata over additional AST mutation.

## Data and Lifetime Contracts

### AST Lifetime

- AST nodes are allocated by parser/AST helpers and freed by `node_free`.
- Checker and codegen never take ownership of AST nodes.

### Type Lifetime

- `Type` objects come from `types.c` allocators and are globally tracked.
- `checker_free` eventually calls `types_cleanup`; any borrowed `Type*` must not outlive checker teardown.

### Module Sources

- `ModuleLoader` owns imported source buffers for token string backing.
- Those buffers must remain alive while parser/checker/codegen use AST token text.

### Checker-to-Codegen Snapshot

- `CodeGenChecker` contains non-owning pointers into checker-managed arrays.
- `codegen_emit` must run before `checker_free`.

## Refactor Guardrails

When making feature or refactor changes:

1. Do not add new parser/checker/codegen cross-phase hidden coupling.
2. Do not add new large switch/god functions when helper extraction is possible.
3. Prefer single source of truth for type lowering/mangling logic.
4. Keep pass scheduling logic centralized (avoid copy/pasted skip predicates).
5. Keep ownership explicit in headers when pointers are borrowed.

## Current Known Debt (Intentional Baseline)

- Some checker paths still mutate AST fields in ways codegen depends on.
- Type lowering/mangling logic is duplicated across checker/codegen utilities.
- Multi-pass checker orchestration currently has repeated filtering predicates.

These are active refactor targets; new code should not deepen these patterns.

## Complexity Baseline Workflow

Use:

```bash
./scripts/complexity_report.sh
```

This prints a threshold report and sorted views for hot functions/files. Use it to
track whether refactors reduce complexity in touched areas.
