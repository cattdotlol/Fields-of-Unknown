#ifndef GFX_LIGHTING_H
#define GFX_LIGHTING_H

#include "raylib.h"

/* Dynamic 2D lighting with real shadows.

   Not ray tracing in the 3D sense - that needs hardware APIs raylib does
   not expose, and it solves a problem a side-on 2D game does not have.
   What it is instead is the 2D equivalent: every light casts rays past
   the corners of nearby solids, and the geometry behind them is dark.
   That is what makes a cave feel like somewhere you went into. */

void LightingLoad(void);
void LightingUnload(void);

/* ambient 0 = pitch black, 1 = fully lit. Everything drawn between Begin
   and End is unaffected; the lightmap is composited in End. */
void LightingBegin(float ambient);
void LightingAddLight(Camera2D camera, Vector2 world, float radius,
                      Color colour, float intensity);
void LightingEnd(void);

#endif /* GFX_LIGHTING_H */
