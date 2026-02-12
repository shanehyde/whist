# AGENTS.md

## Team Conventions

- "issue #N" means GitHub issue `N` in `shanehyde/whist`.
- Prefer GitHub MCP tools over `gh` CLI for all GitHub operations.
- Only use `gh` if MCP is unavailable or lacks the required operation.
- When issue numbers are mentioned, fetch issue `N` via GitHub MCP first before responding.
- If there is ambiguity, prefer GitHub issue context over local docs.

## Quick Repo Map

- `w0/` — active compiler source, build, and test runner.
- `w0/test/` — language/compiler tests (`error_*.w` are expected-failure tests).
- `lib/` — standard library modules and C headers used with `--lib-path`.
- `features/` — design docs and future plans.
- `grammar.md` — language grammar and supported syntax/semantics.
- `PROGRESS.md` — implemented work and PR/issue history.
- `README.md` and `w0/README.md` — top-level and compiler-specific usage.

## Fast Defaults

- Start implementation/debugging work in `w0/`.
- For behavior changes, add/update tests in `w0/test/` first or alongside code.
- Use `cd w0 && make` to build and `cd w0 && ./scripts/run_tests.sh --valid --errors` to validate.
- Before final handoff, always run `cd w0 && make format`, then `cd w0 && make`, then `cd w0 && ./scripts/run_tests.sh --valid --errors`.

## Commit Style

- Use Conventional Commits.
- Format: `<type>(<scope>): <short summary>`.
- Preferred types: `feat`, `fix`, `refactor`, `docs`, `test`, `chore`.
- Keep subject line imperative and concise.
- Example: `feat(casts): add struct->voidptr and voidptr->u64 casts`.
