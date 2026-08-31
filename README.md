# Fields of Unknown

A feral cat wakes on a planet nobody charted, in an overgrown industrial
sprawl that is half underwater and still raining.

No tutorial, no markers, no quest log. Nothing tells you that you are
hungry, or what the thing across the water is. You find out.

C11 and [raylib](https://www.raylib.com/) 5.5.

> Early days. World, weather, movement and food are in. Nothing hunts the
> cat back yet.

## Build

```sh
sudo dnf install raylib-devel        # or: apt install libraylib-dev
make run
```

Other targets: `release`, `test`, `windows` (needs `mingw64-gcc`),
`dist`, `dist-windows`.

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
| `F1` `F5` | debug overlay, reroll world |

Rebindable in Settings → Controls.

## Notes

The world is endless and stored nowhere: each chunk is a pure function of
`(seed, index)`, so walking back returns you to the same place. Generation
reads the cat's real jump reach and rejects anything it cannot cross.

Rats hear rather than see. A sprint clears the street; a crouch gets you
close. Rain masks noise but raises the water, which drowns the ground
route and pushes you up onto the ledges.

## Licence

Not yet decided.
