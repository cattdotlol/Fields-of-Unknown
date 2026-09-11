# Working in Fields of Unknown

This is a C11/raylib survival game. Read this file first, then
[docs/architecture.md](docs/architecture.md) for the subsystem you will change.
The user's requested outcome defines scope. Implement and verify authorized
work without requiring approval for routine local edits.

## Start here

1. Run `git status --short`; preserve changes that predate your task.
2. Run `python3 tools/dev.py doctor`. It reports missing prerequisites without
   installing software or changing the system. See [development.md](docs/development.md).
3. Read the affected implementation, public header, and relevant tests. Use
   `rg` to find callers before changing an interface.
4. Make a focused change and exercise it through real public functions.
5. Run `python3 tools/dev.py check` and `git diff --check`. For physics,
   indexing, allocation, or lifetime changes, also use `--sanitizers` where
   the runtime is installed. A missing tool is a blocked check, never a pass.
6. Report the changed behavior, commands actually run, results, and remaining
   limitations. Logs and machine-readable results live in `build/checks/`.

## Engineering constraints

- Keep C11 portability across Linux, macOS, and the Windows cross build.
- Simulation uses fixed ticks. Frame input is for menus; use
  `InputConsumePressed` for one-shot simulation actions. Clear pending input
  when gameplay is blocked or a run resets.
- Bodies use bottom-center coordinates, positive Y downward. Call
  `BodyBeginTick` before moving and interpolate only for rendering.
- World generation must remain a function of seed and chunk index. Use
  module-owned `Rng` streams; do not add wall-clock randomness to simulation.
- Register creature lifecycle hooks in `src/entity/agents.c`. The creature
  census reads the preceding tick; consumed entries are hidden immediately.
- Account for reset, update, draw, and teardown when adding state. Keep GPU,
  audio-device, and window calls out of headless test setup.
- Preserve the pixel-art style and shared UI scaling. Visual changes need
  an interactive check; headless tests do not establish visual correctness.
- Keep developer-only gameplay shortcuts behind `NDEBUG` guards.
- Do not edit generated `build/`, `dist/`, or `vendor/` files as source fixes.
  Avoid `make clean` as a default: it deletes build outputs and packages.

## Tests and handoff

`make test TEST_SUITE=physics` builds and runs one named suite. Omitting
`TEST_SUITE` runs all suites. `build/tests/run --list` lists available names.
Register new suites in `tests/tests.h` and the table in `tests/main.c`.
Tests should verify behavior, not copy the implementation's calculations.

For a task spanning multiple sessions, record objective, acceptance checks,
decisions, commands/results, and next steps in a short task note. Never put
credentials or environment dumps in notes. Keep source changes reviewable;
commits, publication, and deployments follow the user's requested scope.

Repository instructions and checks help coding agents work independently;
they do not start an agent service or grant external account access.
