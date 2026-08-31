# Fields of Unknown

[![build](https://github.com/cattdotlol/Fields-of-Unknown/actions/workflows/ci.yml/badge.svg)](https://github.com/cattdotlol/Fields-of-Unknown/actions/workflows/ci.yml)

A cat wakes up on a planet nobody charted. It's flooded, overgrown, and
still raining. Nothing explains anything to you.

C11 + [raylib](https://www.raylib.com/). Work in progress.

## Build

```sh
sudo dnf install raylib-devel      # apt: libraylib-dev
make run
```

`make test` runs the tests. `make windows` cross-compiles (needs
mingw64-gcc). `make dist` and `make dist-windows` package it up.

If you're sending a build to someone, note that Linux binaries don't move
between distros — glibc only resolves forward. Send them the source, or
have CI build it.

## Controls

| | |
|---|---|
| `A` `D` | move |
| `Shift` | run |
| `Ctrl` | sneak |
| `Space` | jump |
| `Down` (in water) | dive |
| `E` | eat |
| `Esc` | menu |
| `~` | dev tools (debug builds only) |

All rebindable in Settings → Controls.

## Licence

Haven't picked one yet.
