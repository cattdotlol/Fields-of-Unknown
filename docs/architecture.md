# Architecture and change map

The executable enters through `src/main.c`. `core/app.c` initializes raylib,
loads shared resources, registers screens, and runs fixed simulation ticks
followed by frame updates and rendering. `core/timestep.c` caps catch-up work
while preserving the fractional tick used for interpolation.

| Area | Start here | Responsibilities / tests |
| --- | --- | --- |
| Input | `src/core/input.c` | Keyboard/controller bindings, scripted input, frame edges, pending simulation presses; `input` suite |
| Screens | `src/screens/screens.h`, `screen_gameplay.c` | Screen callbacks, run resets, camera, death fade, gameplay update order |
| Movement | `src/world/physics.c`, `src/entity/cat.c` | Axis sweeps, contact resolution, jump/swim logic, interpolated bodies; `physics` suite |
| World | `src/world/worldgen.c`, `terrain.c` | Seeded chunks, streaming, solid geometry, district layout; `worldgen`, `district`, `tree` suites |
| Environment | `src/world/weather.c`, `daylight.c`, `season.c`, `ocean.c` | Environmental state and survival conditions; `daylight`, `ocean`, `vitals` suites |
| Creature lifecycle | `src/entity/agents.c` | Reset/tick/draw registry for rat, aquatic, and stalker modules; `agents` suite |
| Creature interactions | `src/entity/creatures.c`, `species.c` | Double-buffered census, diet queries, consumption; `creatures`, `species` suites |
| Survival | `src/entity/vitals.c`, `src/world/mushroom.c` | Health, hunger, stamina, warmth, breath, nutrition; `vitals`, `mushroom` suites |
| Rendering | `src/gfx/`, `src/entity/cat_art.c` | Lighting, backdrop, film effects, sprite art; interactive verification required |
| UI and settings | `src/ui/`, `src/core/settings.c` | Shared sizing/widgets, HUD, persistent settings and bindings |
| Build and validation | `Makefile`, `tools/dev.py`, `.github/workflows/ci.yml` | Isolated configurations, local checks, CI, packaging |

## Tick and state ownership

The app advances season, weather, daylight, and the backdrop before the
active screen's fixed update. Gameplay then opens the creature census,
updates the cat, streams terrain, updates camera/light adaptation and
vitals, ticks mushrooms and creatures, and processes eating. Preserve this
ordering unless the change explicitly includes its consequences.

`CreaturesBeginTick` exchanges read/write buffers. Queries see the previous
tick, while modules publish into the current one. References into the census
must not survive buffer reuse. Removal callbacks belong to species modules;
nutrition is granted only when consumption succeeds.

Most subsystems own static singleton state. They are not isolated game
instances and are not safe to run concurrently within one process. Use
separate processes for independent simulations. Tests must explicitly reset
the state they use; run the full suite as well as focused suites to find
state leakage.

## Common changes

- **New creature:** add species traits/diet as needed, implement its lifecycle,
  register in `agents.c`, publish observations, register removal behavior,
  and test reset/population/interactions. Rendering order is registry order.
- **New input action:** update the enum, designated action labels, defaults,
  persistence compatibility, UI exposure, and tests. Existing settings store
  actions by numeric index; inserting enum values can change saved bindings.
- **Physics tuning:** use actual `BodyMove` or `BodyMoveWithSolids` behavior in
  tests, check both motion directions and contacts, then play-test movement.
- **New screen:** add its ID, callback table, app registration, resource
  lifecycle, and transition behavior. Some existing widgets process input
  during drawing; do not assume all draw callbacks are pure.
- **Determinism changes:** document intentional seed-output changes. RNG
  precision and call order are part of generation behavior.

The simulation remains tied to raylib types and some shared modules. Headless
tests skip opening devices; they still require raylib at build and load time.
