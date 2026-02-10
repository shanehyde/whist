# Changelog

## [Unreleased]

### Changed

- Extracted type query functions from `codegen_emit.c` into new `codegen_types.c/h`
  - Moved 8 functions: `is_enum_type_name`, `enum_index`, `enum_has_rc_fields`, `resolve_alias`, `is_struct_type`, `type_node_has_rc`, `resolve_enum_name`, `codegen_is_type_variable`
  - Created `codegen_types.h` consolidating type query declarations and shared `codegen.c` helper declarations (previously split across `codegen_emit.h` and `codegen_internal.h`)
  - `codegen_emit.c` is now purely emission logic (~750 lines)
- Refactored `CodeGen` struct into 7 inline sub-structures (`out`, `defer`, `rc`, `generics`, `checker`, `enums`, `aliases`) for better readability and organization
  - Introduced named types `RcVar` and `NameAlias` replacing anonymous structs
  - Simplified `codegen_init` to accept `CodeGenChecker` as a value struct with designated initializers
  - ~419 field access sites updated across codegen.c, codegen_emit.c, codegen_expr.c, codegen_stmt.c, codegen_rc.c, and main.c
  - Zero behavior change — purely structural reorganization
