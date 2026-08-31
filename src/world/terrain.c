#include "world/terrain.h"
#include "world/weather.h"
#include "world/worldgen.h"
#include "world/season.h"
#include "world/mushroom.h"

#include <math.h>

static Chunk sChunks[TERRAIN_LOADED_CHUNKS];
static int   sCentre = -99999;
static bool  sPrimed;

/* Flattened solids, rebuilt only when the window moves. Physics walks
   this every tick, so it should not chase chunk structs. */
static Rectangle sFlat[TERRAIN_LOADED_CHUNKS * CHUNK_MAX_SOLIDS];
static int sFlatCount;

static void Flatten(void)
{
    sFlatCount = 0;

    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].solidCount; i++)
        {
            sFlat[sFlatCount++] = sChunks[c].solids[i];
        }
    }
}

void TerrainStream(float centreX)
{
    int centre = (int)floorf(centreX / CHUNK_WIDTH);
    if (sPrimed && centre == sCentre) return;

    int half = TERRAIN_LOADED_CHUNKS / 2;

    Chunk next[TERRAIN_LOADED_CHUNKS];
    for (int i = 0; i < TERRAIN_LOADED_CHUNKS; i++) next[i].active = false;

    for (int i = 0; i < TERRAIN_LOADED_CHUNKS; i++)
    {
        int want = centre - half + i;
        bool reused = false;

        /* Keep chunks that are still in range rather than regenerating
           them - they are identical either way, this is just cheaper. */
        for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
        {
            if (sPrimed && sChunks[c].active && sChunks[c].index == want)
            {
                next[i] = sChunks[c];
                reused = true;
                break;
            }
        }

        if (!reused) WorldBuildChunk(want, &next[i]);
    }

    for (int i = 0; i < TERRAIN_LOADED_CHUNKS; i++) sChunks[i] = next[i];

    sCentre = centre;
    sPrimed = true;
    Flatten();
}

int TerrainCount(void)
{
    return sFlatCount;
}

Rectangle TerrainSolid(int index)
{
    return sFlat[index];
}

bool TerrainOverlaps(Rectangle box)
{
    for (int i = 0; i < sFlatCount; i++)
    {
        if (CheckCollisionRecs(box, sFlat[i])) return true;
    }

    return false;
}

int TerrainLoadedChunks(void)
{
    int n = 0;
    for (int i = 0; i < TERRAIN_LOADED_CHUNKS; i++) if (sChunks[i].active) n++;
    return n;
}

void TerrainLoadedRange(int *first, int *last)
{
    int half = TERRAIN_LOADED_CHUNKS / 2;

    if (first) *first = sCentre - half;
    if (last)  *last  = sCentre + half;
}

/* --- trees -------------------------------------------------------------
   Drawn rather than stored as pixels: trunk, a few branches, and canopy
   clumps whose placement comes from the tree's variant, so a given tree
   looks the same every time you walk past it. */

static unsigned int TreeHash(unsigned int v, unsigned int i)
{
    unsigned int h = (v + 0x9E3779B9u) * 2654435761u ^ (i + 1u) * 2246822519u;
    h ^= h >> 15;
    return h;
}

static float TreeRand(unsigned int v, unsigned int i)
{
    return (float)(TreeHash(v, i) & 0xFFFFu) / 65535.0f;
}

static void DrawTree(const Tree *t)
{
    Color bark   = (Color){ 22, 20, 26, 255 };
    Color barkLo = (Color){ 14, 13, 18, 255 };
    Color leaf, leafHi;
    SeasonFoliage(&leaf, &leafHi);
    Color alien  = (Color){ 58, 32, 72, 255 };

    /* Bare either because this tree is dead, or because it is winter. */
    bool bare = t->dead || (TreeRand(t->variant, 999u) < SeasonBareness());

    float topY = t->baseY - t->height;

    /* Trunk, leaning slightly by variant so a row of them is not a fence. */
    float lean = (TreeRand(t->variant, 0) - 0.5f) * t->height * 0.12f;

    for (float y = t->baseY; y > topY; y -= 4.0f)
    {
        float f = (t->baseY - y) / t->height;
        float w = 12.0f * (1.0f - f * 0.55f);
        float x = t->x + lean * f;

        DrawRectangleRec((Rectangle){ x - w * 0.5f, y - 4.0f, w, 4.0f },
                         (f > 0.5f) ? bark : barkLo);
    }

    /* Branches. */
    int branches = 2 + (int)(TreeRand(t->variant, 1) * 3.0f);
    for (int b = 0; b < branches; b++)
    {
        float f = 0.45f + TreeRand(t->variant, 10u + (unsigned int)b) * 0.5f;
        float dir = (TreeRand(t->variant, 20u + (unsigned int)b) < 0.5f) ? -1.0f : 1.0f;
        float len = t->spread * (0.5f + TreeRand(t->variant, 30u + (unsigned int)b) * 0.7f);

        float y = t->baseY - t->height * f;
        float x = t->x + lean * f;

        for (float s = 0.0f; s < len; s += 4.0f)
        {
            DrawRectangleRec((Rectangle){ x + dir * s, y - s * 0.45f, 5.0f, 5.0f }, bark);
        }
    }

    if (bare) return;

    /* Canopy: clumps, not a circle. */
    int clumps = 5 + (int)(TreeRand(t->variant, 2) * 5.0f);
    for (int i = 0; i < clumps; i++)
    {
        unsigned int k = 40u + (unsigned int)i;
        float ox = (TreeRand(t->variant, k) - 0.5f) * t->spread * 2.0f;
        float oy = (TreeRand(t->variant, k + 100u) - 0.5f) * t->spread * 1.1f;
        float r  = t->spread * (0.35f + TreeRand(t->variant, k + 200u) * 0.45f);

        Color c = leaf;
        if (TreeRand(t->variant, k + 300u) > 0.78f) c = alien;
        else if (TreeRand(t->variant, k + 400u) > 0.6f) c = leafHi;

        DrawRectangleRec((Rectangle){ t->x + lean + ox - r, topY + oy - r * 0.7f,
                                      r * 2.0f, r * 1.4f }, c);
    }
}

/* Reach is generous: this is a cat nosing at the ground, not a cursor. */
static bool Touching(Rectangle box, const Mushroom *m)
{
    float pad = 18.0f;

    return (m->x >= box.x - pad && m->x <= box.x + box.width + pad &&
            m->baseY >= box.y - pad && m->baseY <= box.y + box.height + pad * 2.0f);
}

int TerrainMushroomUnder(Rectangle box)
{
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].mushroomCount; i++)
        {
            if (MushroomIsHarvested(sChunks[c].index, i)) continue;
            if (!Touching(box, &sChunks[c].mushrooms[i])) continue;

            return (int)sChunks[c].mushrooms[i].species;
        }
    }

    return -1;
}

int TerrainEatAt(Rectangle box)
{
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].mushroomCount; i++)
        {
            if (MushroomIsHarvested(sChunks[c].index, i)) continue;
            if (!Touching(box, &sChunks[c].mushrooms[i])) continue;

            MushroomHarvest(sChunks[c].index, i);
            return (int)sChunks[c].mushrooms[i].species;
        }
    }

    return -1;
}

void TerrainDraw(float left, float right, Rectangle focus)
{
    Color body = (Color){ 26, 30, 38, 255 };
    Color lip  = (Color){ 44, 52, 60, 255 };
    Color moss = (Color){ 34, 58, 40, 255 };

    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].treeCount; i++)
        {
            const Tree *t = &sChunks[c].trees[i];
            if (t->x + t->spread * 2.0f < left || t->x - t->spread * 2.0f > right) continue;

            DrawTree(t);
        }
    }

    for (int i = 0; i < sFlatCount; i++)
    {
        Rectangle r = sFlat[i];
        if (r.x + r.width < left || r.x > right) continue;

        DrawRectangleRec(r, body);
        DrawRectangle((int)r.x, (int)r.y, (int)r.width, 3, lip);

        for (float x = r.x + 4.0f; x < r.x + r.width - 4.0f; x += 17.0f)
        {
            if (sinf(x * 0.37f) < 0.35f) continue;

            DrawRectangle((int)x, (int)(r.y - 3.0f), 3, 3, moss);
        }
    }

    /* Mushrooms last, so they sit on top of the ground they grow from. */
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].mushroomCount; i++)
        {
            const Mushroom *m = &sChunks[c].mushrooms[i];

            if (m->x < left - 40.0f || m->x > right + 40.0f) continue;
            if (MushroomIsHarvested(sChunks[c].index, i)) continue;

            MushroomDraw(m, Touching(focus, m));
        }
    }
}

void TerrainDrawWater(float left, float right)
{
    float y = WeatherWaterY();
    float w = right - left;

    DrawRectangle((int)left, (int)y, (int)w, 2, (Color){ 96, 150, 158, 190 });
    DrawRectangle((int)left, (int)(y + 2.0f), (int)w, 900,
                  (Color){ 14, 40, 48, 165 });
}
