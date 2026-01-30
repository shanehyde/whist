# AI Agent Instructions

This file provides concise instructions for automated agents operating on this repository.

---

ai_agent_integration:
working_directory: "w0"
purpose: "Indicate that the active development and build happens in the `w0/` subfolder"
entrypoint: "w0/bin/w0 (built by `make` in `w0/`)"
recommended_commands:

- "cd w0"
- "make"
- "make test" # Enhanced test suite with colors and comprehensive reporting
- "make test-valid" # Run only valid program tests
- "make test-errors" # Run only error case tests
- "make test-verbose" # Detailed test output for debugging
- "make test-help" # Show available test commands
- "./bin/w0 --ast test/functions.w"
- "./bin/w0 --check test/\*.w"
  allowed_actions:
  read_files: true
  run_commands: true
  modify_docs: true
  modify_code: false # request explicit permission before changing source code
  primary_paths: - "w0/" - "w0/test/"
  notes: |
  Agents may run the build and test commands inside `w0/` to verify behaviour.
  If source code changes are required, always request explicit user approval
  before editing files or creating commits/PRs.

TEST SUITE ENHANCEMENTS (January 2026):

- Enhanced test runner with color-coded output (✓ PASS, ✗ FAIL)
- Comprehensive error validation ensuring error tests fail with expected messages
- Multiple test targets: test, test-valid, test-errors, test-verbose, test-help
- Proper exit codes (0 success, 1 failure) for CI/CD integration
- Summary statistics showing pass/fail counts for valid and error test cases
- Tests organized as: test/_.w (valid programs), test/error\__.w (should fail)

---

Short guidance

- Work from the `w0/` directory: all build, test, and runtime actions should be
  performed there. Use `cd w0` first.
- Use `make` to build `bin/w0` and `make test` to run the enhanced test suite.
- Test suite provides color-coded feedback with comprehensive error validation.
- Use `make test-help` to see all available test commands and options.
- For quick inspections use `./bin/w0 --ast <file.w>` or `--check` to type-check.
- Read `w0/CLAUDE.md` for more detailed project and language information.
- Safety: do not modify source files or push commits unless the user explicitly
  authorises changes. Modifying documentation is allowed.

If you need permission to make changes, ask the repository owner or maintainer.
