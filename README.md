# Fields of Unknown

[![build](https://github.com/cattdotlol/Fields-of-Unknown/actions/workflows/ci.yml/badge.svg)](https://github.com/cattdotlol/Fields-of-Unknown/actions/workflows/ci.yml)

A feral cat wakes on a planet nobody charted, in an overgrown industrial
sprawl that is half underwater and still raining.

No tutorial, no markers, no quest log. Nothing tells you that you are
hungry, or what the thing across the water is. You find out.

C11 and [raylib](https://www.raylib.com/) 5.5.

> Early days. World, weather, movement, food and something that hunts you
> are in. No shelter, and dying costs nothing yet.

## Build

```sh
sudo dnf install raylib-devel        # or: apt install libraylib-dev
make run
```

Other targets: `release`, `test`, `windows` (needs `mingw64-gcc`),
`dist`, `dist-windows`, `vars`.

Linux, macOS and Windows builds run on every push; see Actions for
artifacts.

Linux binaries do not travel between distributions — glibc versions only
resolve forward. Build it where you intend to play it, or send someone
`make dist-src`.

## Controls

| | |
|---|---|
| `A` `D` | move |
| `Shift` | run |
| `Ctrl` | sneak |
| `Space` | jump |
| `E` | eat |
| `Down` in water | dive |
| `~` | dev menu (debug builds) |
| `F1` `F5` | debug overlay, reroll world |

Rebindable in Settings → Controls.

## Notes

The world is endless and stored nowhere: each chunk is a pure function of
`(seed, index)`, so walking back returns you to the same place. Generation
reads the cat's real jump reach and rejects anything it cannot cross.

It is divided into districts six chunks wide, so a city is somewhere you
arrive at rather than a building that happens to be here. Cities have
apartment blocks you can walk into and climb; the wild is hollow
underneath, with caves reached through shafts in the ground; crash sites
are strewn with hull plate off the ship.

Rats hear rather than see. A sprint clears the street; a crouch gets you
close. Rain masks noise but raises the water, which drowns the ground
route and pushes you up onto the ledges.

Lights cast real shadows: every light rays past the corners of nearby
solids, so a cave is lit by what you brought into it. The cat sees in the
dark, apartment windows do not, and the thing hunting you glows.

Something else hunts the same signal the rats run from. It is slower than
a sprinting cat, will not cross water, and forgets you if it cannot find
you — so noise is the whole conversation.

## Licence

Not yet decided.
