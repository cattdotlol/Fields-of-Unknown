# Repeatable development

Use Python 3, GNU Make, a C11 compiler, and raylib 5.5 with its platform
development libraries. `tools/dev.py` uses only Python's standard library.
Linux and macOS run native checks; Windows is covered by the existing MinGW
cross-build CI job.

```sh
python3 tools/dev.py doctor
python3 tools/dev.py check
python3 tools/dev.py check --sanitizers
```

`doctor` checks tool availability and raylib discovery. It does not install
packages, open a window, or prove compilation will succeed. If raylib is
provided outside pkg-config, export `RAYLIB_CFLAGS` and `RAYLIB_LIBS`. The
Makefile also accepts `SYS_LIBS` for platform linking; CI shows the vendored
static-raylib configuration. Export `CC` to choose a compiler.

`check` runs tooling tests, a forced release rebuild with warnings treated
as errors, debug tests, and optimized release tests. `--sanitizers` also runs
tests instrumented with AddressSanitizer and UndefinedBehaviorSanitizer;
sanitizer diagnostics fail the command. Each step has a 600-second timeout,
overridable with `--timeout`. Checks run sequentially and stop at the first
failure. No installation, commit, push, or release is performed.

Stdout contains a JSON report with schema version 1. Progress goes to stderr.
Exit codes are 0 (passed), 1 (failed), and 2 (missing prerequisites). For
`check`, the same report is saved to `build/checks/summary.json`, with logs
for executed steps alongside it. Earlier logs may remain; the current
summary is authoritative about which checks ran. Treat skipped checks as
unverified. CI uploads these files even on failures.

## Fast feedback

```sh
make test TEST_SUITE=input
make test TEST_SUITE=physics
build/tests/run --list
make test-release TEST_SUITE=input
make test-asan TEST_SUITE=physics
python3 -m unittest discover -s tools -p 'test_*.py'
git diff --check
```

An unknown suite returns an error rather than silently running zero tests.
Tooling tests can run without the C toolchain. Focused suites accelerate
iteration; the full check remains the completion check for code changes.

## Editor and code-navigation support

```sh
python3 tools/dev.py compdb
```

This asks Make for the debug compilation commands without compiling and
writes `compile_commands.json` for tools such as clangd. It preserves the
actual include paths and flags from Make rather than maintaining a second
build description. Regenerate after changing sources, flags, or raylib
location. The generated file is ignored by Git.

## Build outputs and interactive checks

`make debug`, `make release`, and `make asan` keep separate artifacts under
`build/debug`, `build/release`, and `build/asan`. Each also copies its binary
to `build/game` for compatibility. Avoid requesting multiple configurations
in a single parallel Make invocation because that compatibility copy is
shared. `make run` launches the debug binary from the repository root.

For movement, camera, UI, or lighting changes, test a new run, move/jump/swim,
resize the window, and enter/leave menus as appropriate. Record what you
actually exercised. CI's headless checks cannot validate rendering or feel.

## Task handoff template

```text
Objective:
Acceptance checks:
Relevant files / invariants:
Implemented:
Validation commands and results:
Known limitations:
Next action:
```

Keep task notes specific to the work. Do not convert guesses or stale
environment failures into permanent architectural rules.
