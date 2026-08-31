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
bool      TerrainOverlaps(Rectangle box);

int  TerrainLoadedChunks(void);
void TerrainLoadedRange(int *first, int *last);

void TerrainDraw(float left, float right);
void TerrainDrawWater(float left, float right);

#endif /* WORLD_TERRAIN_H */
