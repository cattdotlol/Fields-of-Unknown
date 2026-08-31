#include "world/terrain.h"
#include "world/weather.h"
#include "world/worldgen.h"
#include "world/season.h"
#include "world/tree.h"
#include "world/weather.h"
#include "world/mushroom.h"
#include "world/worldgen.h"

#include <math.h>

static Chunk sChunks[TERRAIN_LOADED_CHUNKS];
static int   sCentre = -99999;
static bool  sPrimed;

/* Flattened solids, rebuilt only when the window moves. Physics walks
   this every tick, so it should not chase chunk structs. */
static Rectangle sFlat[TERRAIN_LOADED_CHUNKS * CHUNK_MAX_SOLIDS];
static unsigned char sFlatKind[TERRAIN_LOADED_CHUNKS * CHUNK_MAX_SOLIDS];
static int sFlatCount;

static void Flatten(void)
{
    sFlatCount = 0;

    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].solidCount; i++)
        {
            sFlatKind[sFlatCount] = sChunks[c].kinds[i];
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

int TerrainSolidKind(int index)
{
    return (int)sFlatKind[index];
}

int TerrainVentCount(void)
{
    int n = 0;
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (sChunks[c].active) n += sChunks[c].ventCount;
    }
    return n;
}

Vector2 TerrainVent(int index)
{
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        if (index < sChunks[c].ventCount) return sChunks[c].vents[index];
        index -= sChunks[c].ventCount;
    }

    return (Vector2){ 0.0f, 0.0f };
}

bool TerrainAirAt(Vector2 point)
{
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].airCount; i++)
        {
            Rectangle r = sChunks[c].air[i];

            if (point.x >= r.x && point.x <= r.x + r.width &&
                point.y >= r.y && point.y <= r.y + r.height) return true;
        }
    }

    return false;
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

/* How far a point on a tree is pushed by the wind.

   Displacement is a function of height above the base, so connected
   segments stay connected - offsetting endpoints individually would pull
   the tree apart. It goes with the square of height, which is roughly how
   a tapering column bends: the trunk barely moves and the tips move a
   lot. The height term inside the sine delays the motion further up, so
   a gust visibly travels through the crown instead of the whole tree
   twitching at once. */
static float TreeSway(const Tree *t, float y, float phase, float flutter)
{
    float h = (t->baseY - y) / (t->height > 1.0f ? t->height : 1.0f);
    if (h < 0.0f) h = 0.0f;
    if (h > 1.4f) h = 1.4f;

    float wind = WeatherWind();
    float bend = h * h;

    float time = (float)GetTime();

    float lean = wind * 15.0f;
    float swing = wind * 11.0f * sinf(time * 1.6f + phase - h * 1.8f);

    /* Thin outer growth flutters faster than the branch carrying it. */
    float shiver = flutter * (0.6f + fabsf(wind)) * sinf(time * 5.2f + phase + h * 4.0f);

    return bend * (lean + swing) + shiver;
}

static void DrawTree(const Tree *t)
{
    TreeBranch branches[TREE_MAX_BRANCHES];
    int count = TreeBuild(t, branches, TREE_MAX_BRANCHES);

    float phase = (float)(t->variant & 1023u) * 0.0061f;

    TreeSpecies species = TreeSpeciesOf(t);

    Color leaf, leafHigh;
    SeasonFoliage(&leaf, &leafHigh);

    bool bare = t->dead || (TreeRand(t->variant, 999u) < SeasonBareness());

    /* Branches first, drawn as squares along each segment so they keep
       the same chunky grain as everything else. */
    for (int i = 0; i < count; i++)
    {
        TreeBranch *b = &branches[i];

        float dx = b->to.x - b->from.x;
        float dy = b->to.y - b->from.y;
        float len = sqrtf(dx * dx + dy * dy);
        if (len < 0.5f) continue;

        float thick = b->thickness;
        if (thick < 2.0f) thick = 2.0f;

        float step = (thick > 5.0f) ? 3.0f : 4.0f;

        for (float d = 0.0f; d <= len; d += step)
        {
            float f = d / len;

            /* Taper along the segment, the way a real branch does. */
            float w = thick * (1.0f - f * 0.25f);

            float py = b->from.y + dy * f;
            float px = b->from.x + dx * f + TreeSway(t, py, phase, 0.0f);

            DrawRectangleRec((Rectangle){ px - w * 0.5f, py - w * 0.5f, w, w },
                             TreeBarkColour(species, b->depth));
        }
    }

    if (bare) return;

    /* Foliage in clumps at the branch tips rather than one blob on top. */
    for (int i = 0; i < count; i++)
    {
        if (!branches[i].tip) continue;

        float cx = branches[i].to.x;
        float cy = branches[i].to.y;

        int clumps = 2 + (int)(TreeRand(t->variant, (unsigned int)i) * 3.0f);

        for (int k = 0; k < clumps; k++)
        {
            unsigned int salt = (unsigned int)(i * 8 + k);

            float ox = (TreeRand(t->variant, salt) - 0.5f) * 22.0f;
            float oy = (TreeRand(t->variant, salt + 300u) - 0.5f) * 18.0f;
            float r = 5.0f + TreeRand(t->variant, salt + 600u) * 6.0f;

            Color c = (TreeRand(t->variant, salt + 900u) > 0.55f) ? leafHigh : leaf;

            float ly = cy + oy;
            float lx = cx + ox + TreeSway(t, ly, phase + (float)k * 0.7f, 2.4f);

            DrawRectangleRec((Rectangle){ lx - r, ly - r * 0.75f,
                                          r * 2.0f, r * 1.5f }, c);
        }
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

    /* Decor behind the solids: interiors, windows and columns are what
       a building is mostly made of, and none of them are collided with. */
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int d = 0; d < sChunks[c].decorCount; d++)
        {
            Rectangle r = sChunks[c].decor[d].rect;
            if (r.x + r.width < left || r.x > right) continue;

            switch (sChunks[c].decor[d].kind)
            {
                case DECOR_ROOM:
                    /* Opaque, so the skyline does not show through. */
                    DrawRectangleRec(r, (Color){ 21, 19, 23, 255 });
                    break;

                case DECOR_PILLAR:
                    DrawRectangleRec(r, (Color){ 34, 31, 35, 255 });
                    DrawRectangle((int)r.x, (int)r.y, 2, (int)r.height,
                                  (Color){ 48, 44, 48, 255 });
                    break;

                case DECOR_WINDOW:
                    DrawRectangleRec(r, (Color){ 15, 17, 24, 255 });
                    DrawRectangleLinesEx(r, 2.0f, (Color){ 40, 38, 42, 255 });
                    break;

                case DECOR_WINDOW_LIT:
                {
                    DrawRectangleRec(r, (Color){ 168, 132, 68, 255 });

                    /* A frame, so it reads as a window and not a slab. */
                    DrawRectangle((int)(r.x + r.width * 0.5f - 1.0f), (int)r.y,
                                  2, (int)r.height, (Color){ 40, 32, 22, 255 });
                    DrawRectangleLinesEx(r, 2.0f, (Color){ 46, 40, 34, 255 });
                    break;
                }

                case DECOR_PLINTH:
                    DrawRectangleRec(r, (Color){ 40, 38, 40, 255 });
                    break;

                default: break;
            }
        }
    }

    for (int i = 0; i < sFlatCount; i++)
    {
        Rectangle r = sFlat[i];
        if (r.x + r.width < left || r.x > right) continue;

        switch (sFlatKind[i])
        {
            case SOLID_WALL:
            {
                /* Rendered brick with windows, so a block reads as
                   somewhere that was lived in. */
                DrawRectangleRec(r, (Color){ 46, 42, 44, 255 });
                DrawRectangle((int)r.x, (int)r.y, (int)r.width, 2,
                              (Color){ 66, 60, 62, 255 });

                for (float wy = r.y + 26.0f; wy < r.y + r.height - 24.0f; wy += 80.0f)
                {
                    bool lit = (sinf(wy * 0.21f + r.x * 0.07f) > 0.55f);

                    DrawRectangle((int)(r.x + 3.0f), (int)wy, 8, 14,
                                  lit ? (Color){ 156, 122, 64, 255 }
                                      : (Color){ 18, 20, 26, 255 });
                }
                break;
            }

            case SOLID_FLOOR:
                DrawRectangleRec(r, (Color){ 38, 35, 38, 255 });
                DrawRectangle((int)r.x, (int)r.y, (int)r.width, 2,
                              (Color){ 60, 56, 58, 255 });
                break;

            case SOLID_ROOF:
                DrawRectangleRec(r, (Color){ 28, 26, 30, 255 });
                DrawRectangle((int)r.x, (int)r.y, (int)r.width, 2,
                              (Color){ 52, 48, 52, 255 });
                break;

            case SOLID_DEBRIS:
                /* Hull plate: cold metal with a scorched edge. */
                DrawRectangleRec(r, (Color){ 44, 48, 56, 255 });
                DrawRectangle((int)r.x, (int)r.y, (int)r.width, 2,
                              (Color){ 78, 84, 94, 255 });
                DrawRectangle((int)r.x, (int)(r.y + r.height - 3.0f),
                              (int)r.width, 3, (Color){ 30, 22, 20, 255 });
                break;

            case SOLID_ROCK:
                DrawRectangleRec(r, (Color){ 32, 28, 26, 255 });
                DrawRectangle((int)r.x, (int)r.y, (int)r.width, 2,
                              (Color){ 50, 44, 40, 255 });
                break;

            case SOLID_GROUND:
            case SOLID_LEDGE:
            default:
                DrawRectangleRec(r, body);
                DrawRectangle((int)r.x, (int)r.y, (int)r.width, 3, lip);

                for (float x = r.x + 4.0f; x < r.x + r.width - 4.0f; x += 17.0f)
                {
                    if (sinf(x * 0.37f) < 0.35f) continue;

                    float mossSway = WeatherWind() * 2.5f * sinf((float)GetTime() * 3.0f + x * 0.05f);

                    DrawRectangle((int)(x + mossSway), (int)(r.y - 3.0f), 3, 3, moss);
                }
                break;
        }
    }

    /* Decor in front: things that sit on top of the structure. */
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int d = 0; d < sChunks[c].decorCount; d++)
        {
            Rectangle r = sChunks[c].decor[d].rect;
            if (r.x + r.width < left || r.x > right) continue;

            switch (sChunks[c].decor[d].kind)
            {
                case DECOR_RAILING:
                    DrawRectangleRec(r, (Color){ 52, 50, 54, 255 });
                    break;

                case DECOR_TANK:
                    DrawRectangleRec(r, (Color){ 46, 44, 42, 255 });
                    DrawRectangle((int)r.x, (int)r.y, (int)r.width, 3,
                                  (Color){ 68, 64, 60, 255 });
                    /* Legs, so it stands on the roof rather than floating. */
                    DrawRectangle((int)(r.x + 3.0f), (int)(r.y + r.height), 3, 5,
                                  (Color){ 34, 32, 32, 255 });
                    DrawRectangle((int)(r.x + r.width - 6.0f), (int)(r.y + r.height), 3, 5,
                                  (Color){ 34, 32, 32, 255 });
                    break;

                case DECOR_VENT:
                    DrawRectangleRec(r, (Color){ 54, 56, 58, 255 });
                    for (float vx = r.x + 3.0f; vx < r.x + r.width - 2.0f; vx += 5.0f)
                    {
                        DrawRectangle((int)vx, (int)(r.y + 3.0f), 2, (int)(r.height - 6.0f),
                                      (Color){ 32, 34, 36, 255 });
                    }
                    break;

                case DECOR_ANTENNA:
                    DrawRectangleRec(r, (Color){ 44, 42, 46, 255 });
                    DrawRectangle((int)(r.x - 5.0f), (int)(r.y + 8.0f), 13, 2,
                                  (Color){ 44, 42, 46, 255 });
                    DrawRectangle((int)(r.x - 3.0f), (int)(r.y + 18.0f), 9, 2,
                                  (Color){ 44, 42, 46, 255 });
                    break;

                default: break;
            }
        }
    }

    /* Trapped air reads as a surface from below. */
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].airCount; i++)
        {
            Rectangle r = sChunks[c].air[i];
            if (r.x + r.width < left || r.x > right) continue;

            DrawRectangleRec(r, (Color){ 30, 44, 52, 120 });
            DrawRectangle((int)r.x, (int)(r.y + r.height - 2.0f), (int)r.width, 2,
                          (Color){ 150, 200, 214, 200 });
        }
    }

    /* Vents: a chimney mouth, and what comes out of it. */
    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].ventCount; i++)
        {
            Vector2 v = sChunks[c].vents[i];
            if (v.x < left - 60.0f || v.x > right + 60.0f) continue;

            DrawRectangle((int)(v.x - 9.0f), (int)v.y, 18, 5, (Color){ 62, 40, 34, 255 });

            for (int p = 0; p < 8; p++)
            {
                float t = fmodf((float)GetTime() * 0.35f + (float)p * 0.13f, 1.0f);
                float px = v.x + sinf(t * 5.0f + (float)p) * (6.0f + t * 16.0f);
                float py = v.y - t * 150.0f;
                float side = 3.0f + t * 3.0f;

                DrawRectangleRec((Rectangle){ px, py, side, side },
                                 Fade((Color){ 196, 150, 128, 255 }, (1.0f - t) * 0.40f));
            }
        }
    }

    /* Seaweed, but only the part that is actually under water. */
    float water = WeatherWaterY();
    float t = (float)GetTime();

    for (int c = 0; c < TERRAIN_LOADED_CHUNKS; c++)
    {
        if (!sChunks[c].active) continue;

        for (int i = 0; i < sChunks[c].weedCount; i++)
        {
            const Weed *weed = &sChunks[c].weeds[i];

            if (weed->x < left - 40.0f || weed->x > right + 40.0f) continue;
            if (weed->baseY < water) continue;          /* still dry */

            int blades = 3 + (int)(weed->variant % 3u);

            for (int b = 0; b < blades; b++)
            {
                float ox = ((float)b - (float)blades * 0.5f) * 5.0f;
                float lean = (float)((weed->variant >> (b * 3)) & 7u) * 0.2f;

                for (float d = 0.0f; d < weed->height; d += 4.0f)
                {
                    float top = weed->baseY - d;
                    if (top < water) break;              /* not above the surface */

                    float sway = sinf(t * 0.9f + d * 0.05f + lean + (float)b) * (d * 0.14f);
                    float shade = 0.45f + (d / weed->height) * 0.4f;

                    DrawRectangleRec((Rectangle){ weed->x + ox + sway, top - 4.0f, 3.0f, 4.0f },
                                     Fade((Color){ 38, 96, 66, 255 }, shade));
                }
            }
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

/* --- shelter ------------------------------------------------------------
   Five upward probes, because one ray through a gap between two slabs
   reads as open sky and makes the light flicker. */

#define COVER_RAYS   5
#define COVER_REACH  320.0f

float TerrainCoverAbove(Vector2 p)
{
    float covered = 0.0f;

    for (int i = 0; i < COVER_RAYS; i++)
    {
        float t = (float)i / (float)(COVER_RAYS - 1);
        float ox = (t - 0.5f) * 52.0f;

        for (float h = 24.0f; h <= COVER_REACH; h += 42.0f)
        {
            Rectangle probe = { p.x + ox - 3.0f, p.y - h, 6.0f, 10.0f };

            if (TerrainOverlaps(probe))
            {
                /* A low roof encloses you more than a distant one. */
                covered += 1.0f - (h / COVER_REACH) * 0.45f;
                break;
            }
        }
    }

    return covered / (float)COVER_RAYS;
}
