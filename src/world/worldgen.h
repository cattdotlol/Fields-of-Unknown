#ifndef WORLD_WORLDGEN_H
#define WORLD_WORLDGEN_H

#include "raylib.h"
#include "world/mushroom.h"

#include <stdbool.h>

/* The world is generated one chunk at a time and never ends.

   A chunk's contents are a pure function of (seed, index), so nothing is
   stored, walking back gives you the same place you left, and there is
   no world size to run out of. Continuity between neighbours comes from
   the edge height being derived from the shared boundary index, so both
   sides independently agree on where the ground meets. */

#define CHUNK_WIDTH       1024.0f
#define CHUNK_EDGE_WIDTH   180.0f   /* flat landing either side of a seam */
#define CHUNK_MAX_SOLIDS      44
#define CHUNK_MAX_TREES        6
#define CHUNK_MAX_MUSHROOMS   10

typedef struct Tree {
    float x;            /* trunk centre, world space */
    float baseY;        /* ground it stands on */
    float height;
    float spread;       /* canopy half-width */
    unsigned int variant;
    bool  dead;         /* bare, no canopy */
} Tree;

typedef struct Chunk {
    int   index;
    bool  active;

    Rectangle solids[CHUNK_MAX_SOLIDS];
    int   solidCount;

    Tree  trees[CHUNK_MAX_TREES];
    int   treeCount;

    Mushroom mushrooms[CHUNK_MAX_MUSHROOMS];
    int   mushroomCount;
} Chunk;

void  WorldSetSeed(unsigned int seed);
unsigned int WorldSeed(void);

/* Ground height at the seam between chunk-1 and chunk. Both neighbours
   call this, which is what makes the join continuous. */
float WorldEdgeHeight(int boundaryIndex);

/* Builds chunk `index` into `out`. Deterministic, and validated: if a
   candidate is not crossable it is rerolled before being returned. */
void  WorldBuildChunk(int index, Chunk *out);

/* Left edge to right edge, given the cat's reach. Exposed for tests. */
bool  WorldChunkTraversable(const Chunk *chunk);

Vector2 WorldSpawnPoint(void);

#endif /* WORLD_WORLDGEN_H */
