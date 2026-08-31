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

## A couple of things worth knowing

The world is infinite and isn't stored anywhere. Every chunk is a pure
function of the seed and its index, so walking back gets you the same
place you left. Generation checks the cat's actual jump reach and throws
away anything it can't cross.

Noise is most of the game. Rats run from it, and something else comes
looking for it. Sprinting is loud, crouching isn't, and rain covers you.

## Licence

Haven't picked one yet.
