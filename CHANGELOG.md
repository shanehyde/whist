# Changelog

## [Unreleased]

### Changed

- Refactored `CodeGen` struct into 7 inline sub-structures (`out`, `defer`, `rc`, `generics`, `checker`, `enums`, `aliases`) for better readability and organization
  - Introduced named types `RcVar` and `NameAlias` replacing anonymous structs
  - Simplified `codegen_init` to accept `CodeGenChecker` as a value struct with designated initializers
  - ~419 field access sites updated across codegen.c, codegen_emit.c, codegen_expr.c, codegen_stmt.c, codegen_rc.c, and main.c
  - Zero behavior change — purely structural reorganization
