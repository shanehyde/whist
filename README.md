# Whist

Whist is a small, bootstrap compiler for the Whist programming language. The
bootstrap implementation included in this repository is `w0/` — a C-based
compiler that parses Whist source and generates C code which can be compiled
with a standard C compiler (gcc, clang).

## Quick start

Run the following from the repository root:

```bash
cd w0
make          # build bin/w0
make test     # run type-check and test suite
```

For quick inspections:

```bash
./bin/w0 --ast test/functions.w   # print AST for a test file
./bin/w0 --check test/*.w         # type-check test files
```

## Where to work

All active development and builds happen in the `w0/` directory. See
`AI_AGENTS.md` for machine- and human-readable guidance for automated agents,
and `w0/CLAUDE.md` for detailed project, language, and build information.

## Notes for contributors

- The `w0/` compiler is written in C and outputs C source. Avoid editing
  source files without explicit approval from the repository owner.
- Documentation edits are allowed; request permission before changing code or
  creating commits/PRs that modify implementation files.
