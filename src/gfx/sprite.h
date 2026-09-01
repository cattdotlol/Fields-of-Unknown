#ifndef GFX_SPRITE_H
#define GFX_SPRITE_H

#include "raylib.h"

#include <stdbool.h>
#include <stddef.h>      /* NULL, for palettes that want no context */

/* Sprites are written as text.

   Every creature in the game is a small grid of characters and a function
   that says what colour each character is. Keeping the art in the source
   as readable rows means a new animal is a thing you can draw in a commit
   diff, and it is why there are no image files to load or ship. This is
   the one blitter that draws them; before it existed the cat, the rat and
   the stalker each had their own copy of the same nested loop.

   Everything is authored FACING LEFT by convention. `authored` records
   that per sprite anyway, so art that breaks the convention still mirrors
   correctly instead of silently coming out backwards. */

typedef struct Sprite {
    const char *const *rows;
    int   w;
    int   h;
    float authored;      /* -1 the art faces left, +1 it faces right */
} Sprite;

/* The colour of one cell. A zero-alpha result means the cell is a hole
   and nothing is drawn there - which is also how a second pass over the
   same sprite draws only part of it, the way the stalker's eyes are done.

   `ctx` is whatever the caller handed in, and is how per-individual state
   reaches the palette: which frame this is, whether the eyes are shut. */
typedef Color (*SpritePalette)(char cell, const void *ctx);

typedef struct SpriteStyle {
    float cellW;
    float cellH;
    float facing;
    float fade;          /* multiplies alpha; 1 is opaque         */
    float clipBelowY;    /* skip anything under this; 0 disables  */
} SpriteStyle;

/* x, y is the top-left corner of the grid. */
void SpriteDrawTopLeft(Sprite s, SpritePalette palette, const void *ctx,
                       float x, float y, SpriteStyle style);

/* Standing on its feet: x is the centre column and y the baseline, with
   square cells. This is what every creature that walks or swims wants,
   because it is where its Body already thinks it is. */
void SpriteDrawStanding(Sprite s, SpritePalette palette, const void *ctx,
                        Vector2 at, float pixel, float facing, float fade);

/* Top-left corners, in world space, of every cell holding `cell`, using
   the same placement as SpriteDrawStanding. For effects that have to draw
   outside the grid - a glow around an eye - which the blitter cannot do
   for them. Returns how many were found, writing at most `max`. */
int SpriteCells(Sprite s, Vector2 at, float pixel, float facing,
                char cell, Vector2 *out, int max);

/* Half the drawn width, for deciding whether it is on screen at all. */
float SpriteHalfWidth(Sprite s, float pixel);

#endif /* GFX_SPRITE_H */
