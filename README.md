# Fields of Unknown

A feral cat wakes on a planet nobody charted, in an overgrown industrial
sprawl that is half underwater and still raining.

No tutorial. No markers. No quest log. The game does not tell you that you
are hungry, or what the thing across the water is. You find out.

Written in C11 against [raylib](https://www.raylib.com/) 5.5.

> Early days — the world, weather and movement are in. Survival and
> anything that hunts you are not yet.

---

## Building

Needs a C compiler, `make`, `pkg-config`, raylib, and the X11/OpenGL
development headers.

**Fedora**

```sh
sudo dnf install gcc make pkgconf-pkg-config raylib-devel
```

**Debian / Ubuntu**

```sh
sudo apt install build-essential pkg-config libraylib-dev \
    libgl1-mesa-dev libx11-dev
```

If your distribution does not package raylib, build it from source and
point the Makefile at it:

```sh
make release RAYLIB_CFLAGS="-I/path/to/raylib/src" \
             RAYLIB_LIBS="/path/to/raylib/src/libraylib.a" \
             SYS_LIBS="-lGL -lX11 -lXrandr -lXi -lXcursor -lXinerama -lm -lpthread -ldl -lrt"
```

Then:

```sh
make run        # build and play
make release    # -O2, stripped
make test       # world generation tests
make asan       # address + UB sanitizers (needs libasan, libubsan)
```

Binaries built on one Linux distribution generally will not run on an
older one — glibc symbol versions only resolve forward. Build it where you
intend to play it.

## Controls

| | |
|---|---|
| `A` `D` / arrows | move |
| `Shift` | run |
| `Ctrl` / `S` / `Down` | crouch |
| `Space` | jump |
| `E` / `F` | eat whatever you are standing on |
| `Esc` | back / menu |
| `F1` | debug overlay |
| `F5` | reroll the world |

## How it fits together

```
src/
├── main.c              init, run, shut down
├── core/
│   ├── app.c           fixed-timestep loop, screen stack, transitions
│   ├── input.c         actions, not keycodes — rebindable, gamepad-aware
│   ├── settings.c      persisted to settings.cfg
│   └── sysinfo.c       CPU/GPU/display, shown in settings
├── ui/                 theme, immediate-mode widgets, pixel cursor
├── gfx/                parallax backdrop, letterbox and grain
├── world/
│   ├── worldgen.c      endless chunks, validated for traversability
│   ├── terrain.c       streaming window of chunks, trees
│   ├── physics.c       AABB bodies, axis-separated resolution
│   └── weather.c       rain, and the water level it drives
├── entity/cat.c        movement, states, how much noise it makes
└── screens/            title, intro, settings, gameplay
```

Some of the load-bearing decisions:

**The simulation runs at a fixed 60 Hz** regardless of display rate, with
render interpolation on top. Physics is deterministic and the cat jumps
the same height on a 144 Hz monitor as a 60 Hz one.

**The world is infinite and stored nowhere.** Each 1024-unit chunk is a
pure function of `(seed, index)`, so walking back returns you to the same
place you left. Neighbouring chunks derive the ground height at their
shared boundary from the boundary index, which is how the seams line up
without the two sides ever talking to each other.

**Generation is validated against the cat's actual reach.** `worldgen`
reads `CatMaxJumpHeight()` and `CatMaxRunJumpDistance()` — both derived
from the movement constants — and rejects any chunk it cannot cross.
Change gravity or jump strength and the level generator retunes itself.

**Weather is heard as well as seen.** Rain streams from `assets/` and its
volume follows the storm. Thunder is *generated* at startup rather than
shipped — brown noise under a decay envelope, normalised so it never
clips — so a strike flashes immediately and the rumble arrives a second or
four later depending on how far off it was.

**Weather is a difficulty dial, not scenery.** Rain masks the cat's scent
and noise, but it also raises the water, which drowns the ground route and
forces you up onto the ledges. Standing water fills faster than it drains.

**Nothing is explained in text.** No meters, no tooltips. The cat's
condition should be readable off the cat.

## Tests

```sh
make test
```

Covers the part that can silently ship an unplayable world: every chunk
crossable, seams continuous, generation deterministic and order
independent, no gap wider than the cat can jump, and a long walk that
confirms the streaming window never grows.

## Licence

Not yet decided.
