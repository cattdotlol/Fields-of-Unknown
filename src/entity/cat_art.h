#ifndef ENTITY_CAT_ART_H
#define ENTITY_CAT_ART_H

#include "raylib.h"

#include <stdbool.h>

/* The cat, transcribed pixel-for-pixel from the reference sprite and
   recoloured black. Front-facing sitting pose, 21x21.

   Shared by the intro and by gameplay so there is exactly one cat. */

#define CAT_ART_W 21
#define CAT_ART_H 21

extern const char *const CatArtSit[CAT_ART_H];

/* Column the body is centred on, ignoring the tail, so the sprite lines
   up with a collision box rather than drifting with the tail. */
#define CAT_ART_BODY_COL 7.5f

/* The art is authored FACING LEFT - the tail trails off to the right, so
   the cat is looking the other way. Drawing it unmirrored for a
   right-facing cat gets it backwards. */
#define CAT_ART_AUTHORED_FACING (-1.0f)

Color CatArtColor(char cell);

/* x,y is the sprite's top-left. cellW/cellH allow squash and stretch.
   facing is the direction the cat is travelling; the sprite is mirrored
   whenever that disagrees with CAT_ART_AUTHORED_FACING. eyesOpen 0..1 closes the eyes
   to fur. fade multiplies alpha. */
void CatArtDraw(float x, float y, float cellW, float cellH,
                float facing, float eyesOpen, float fade, float clipBelowY);

/* Generic frame blitter. Used by the sitting pose today; ready for
   side-view frames when they exist. */

void CatArtDrawFrame(const char *const *rows, int w, int h,
                     float x, float y, float cellW, float cellH,
                     float facing, float eyesOpen, float fade, float clipBelowY);

#endif /* ENTITY_CAT_ART_H */
