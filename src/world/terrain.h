#ifndef WORLD_TERRAIN_H
#define WORLD_TERRAIN_H

#include "raylib.h"

#include <stdbool.h>

/* A sliding window of generated chunks around the player. The world is
   endless, so nothing is ever "the whole level" - only what is near
   enough to matter is resident. */

#define TERRAIN_LOADED_CHUNKS 7

/* Generate/retire chunks so the window stays centred on x. */
void TerrainStream(float centreX);

int       TerrainCount(void);
Rectangle TerrainSolid(int index);
int       TerrainSolidKind(int index);

/* Hydrothermal vents in the loaded window, and whether a point is inside
   a pocket of trapped air. */
int     TerrainVentCount(void);
Vector2 TerrainVent(int index);
bool    TerrainAirAt(Vector2 point);
bool      TerrainOverlaps(Rectangle box);

int  TerrainLoadedChunks(void);
void TerrainLoadedRange(int *first, int *last);

/* Species index of a mushroom the box is standing on, or -1. Peek does
   not consume; Eat marks it harvested. */
int  TerrainMushroomUnder(Rectangle box);
int  TerrainEatAt(Rectangle box);

void TerrainDraw(float left, float right, Rectangle focus);
void TerrainDrawWater(float left, float right);

/* How much solid roof sits over a point, 0 open sky to 1 fully enclosed.
   Shelter is read off this: it decides how much light reaches the cat and
   how much of the night's cold does. */
float TerrainCoverAbove(Vector2 p);

#endif /* WORLD_TERRAIN_H */
