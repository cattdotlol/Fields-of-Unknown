#include "world/worldgen.h"
#include "entity/cat.h"

#include <math.h>

#define GROUND_DEPTH     280.0f
#define GROUND_TOP_HIGH  520.0f   /* smallest y: highest ground */
#define GROUND_TOP_LOW   572.0f   /* largest y: floods first    */
#define LEDGE_THICK       16.0f

/* Safety margins against the cat's theoretical maximum. A jump at the
   absolute limit is not one a player lands reliably. */
#define REACH_UP      0.62f
#define REACH_ACROSS  0.70f

#define MAX_ATTEMPTS  16

static unsigned int sSeed = 1u;

/* --- deterministic noise ----------------------------------------------
   Hash-based rather than a seeded global RNG: chunks must be generatable
   in any order, repeatedly, without perturbing anything else. */

static unsigned int Hash(unsigned int a, unsigned int b)
{
    unsigned int h = a * 2654435761u ^ (b + 0x9E3779B9u) * 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

typedef struct Rand { unsigned int state; } Rand;

static Rand RandSeed(unsigned int a, unsigned int b)
{
    Rand r = { Hash(a, b) | 1u };
    return r;
}

static float RandNext(Rand *r)
{
    r->state = r->state * 1664525u + 1013904223u;
    return (float)((r->state >> 8) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

static float RandRange(Rand *r, float lo, float hi)
{
    return lo + RandNext(r) * (hi - lo);
}

static float StepUp(void)     { return CatMaxJumpHeight() * REACH_UP; }
static float StepAcross(void) { return CatMaxRunJumpDistance() * REACH_ACROSS; }

void WorldSetSeed(unsigned int seed) { sSeed = seed; }
unsigned int WorldSeed(void)         { return sSeed; }

float WorldEdgeHeight(int boundaryIndex)
{
    /* Bias toward the middle of the range so seams are rarely extreme. */
    float a = (float)(Hash(sSeed ^ 0xA5A5u, (unsigned int)boundaryIndex) & 0xFFFFu) / 65535.0f;
    float b = (float)(Hash(sSeed ^ 0x5A5Au, (unsigned int)boundaryIndex) & 0xFFFFu) / 65535.0f;

    return GROUND_TOP_HIGH + ((a + b) * 0.5f) * (GROUND_TOP_LOW - GROUND_TOP_HIGH);
}

/* --- traversability ---------------------------------------------------- */

static float GapBetween(Rectangle a, Rectangle b)
{
    if (b.x > a.x + a.width) return b.x - (a.x + a.width);
    if (a.x > b.x + b.width) return a.x - (b.x + b.width);

    return 0.0f;
}

static bool CanReach(Rectangle from, Rectangle to)
{
    if (from.y - to.y > StepUp()) return false;

    return GapBetween(from, to) <= StepAcross();
}

bool WorldChunkTraversable(const Chunk *chunk)
{
    if (chunk->solidCount < 2) return false;

    bool seen[CHUNK_MAX_SOLIDS] = { false };
    int queue[CHUNK_MAX_SOLIDS];
    int head = 0, tail = 0;

    /* Edge slabs are always emitted first and last. */
    seen[0] = true;
    queue[tail++] = 0;

    while (head < tail)
    {
        int current = queue[head++];
        if (current == 1) return true;          /* reached the right edge */

        for (int i = 0; i < chunk->solidCount; i++)
        {
            if (seen[i]) continue;
            if (!CanReach(chunk->solids[current], chunk->solids[i])) continue;

            seen[i] = true;
            queue[tail++] = i;
        }
    }

    return false;
}

/* --- generation -------------------------------------------------------- */

static bool Add(Chunk *c, Rectangle r)
{
    if (c->solidCount >= CHUNK_MAX_SOLIDS) return false;

    c->solids[c->solidCount++] = r;
    return true;
}

static void AddTree(Chunk *c, Rand *rnd, float x, float baseY)
{
    if (c->treeCount >= CHUNK_MAX_TREES) return;

    Tree *t = &c->trees[c->treeCount++];

    t->x = x;
    t->baseY = baseY;
    t->height = RandRange(rnd, 90.0f, 210.0f);
    t->spread = RandRange(rnd, 26.0f, 58.0f);
    t->variant = (unsigned int)(RandNext(rnd) * 4096.0f);
    t->dead = (RandNext(rnd) < 0.35f);
}

/* A short ascending chain of ledges, each within one jump of the last -
   the route that stays dry when the ground goes under. */
static void LedgeChain(Chunk *c, Rand *rnd, float x, float baseTop, int links)
{
    float top = baseTop;

    for (int i = 0; i < links; i++)
    {
        top -= RandRange(rnd, StepUp() * 0.72f, StepUp() * 0.96f);
        if (top < 220.0f) return;

        float width = RandRange(rnd, 90.0f, 165.0f);
        if (!Add(c, (Rectangle){ x, top, width, LEDGE_THICK })) return;

        x += RandRange(rnd, width * 0.55f, width + StepAcross() * 0.55f);
    }
}

static void BuildOnce(int index, unsigned int salt, Chunk *out)
{
    out->index = index;
    out->solidCount = 0;
    out->treeCount = 0;

    float x0 = (float)index * CHUNK_WIDTH;
    float leftTop  = WorldEdgeHeight(index);
    float rightTop = WorldEdgeHeight(index + 1);

    Rand rnd = RandSeed(sSeed ^ salt, (unsigned int)(index * 2654435761u));

    /* Index 0 and 1 must be the edge slabs: traversal starts and ends
       there, and neighbours butt against them. */
    Add(out, (Rectangle){ x0, leftTop, CHUNK_EDGE_WIDTH, GROUND_DEPTH });
    Add(out, (Rectangle){ x0 + CHUNK_WIDTH - CHUNK_EDGE_WIDTH, rightTop,
                          CHUNK_EDGE_WIDTH, GROUND_DEPTH });

    AddTree(out, &rnd, x0 + RandRange(&rnd, 20.0f, CHUNK_EDGE_WIDTH - 20.0f), leftTop);

    /* Interior: segments and channels between the two edges. */
    float x = x0 + CHUNK_EDGE_WIDTH;
    float endX = x0 + CHUNK_WIDTH - CHUNK_EDGE_WIDTH;
    float top = leftTop;

    while (x < endX - 40.0f)
    {
        bool wide = (RandNext(&rnd) < 0.24f);
        float gap = wide ? RandRange(&rnd, 170.0f, 260.0f)
                         : RandRange(&rnd, 70.0f, StepAcross() * 0.92f);

        if (wide)
        {
            /* A real swim, but always bridged within reach of both banks. */
            float bridgeTop = top - StepUp() * 0.85f;
            Add(out, (Rectangle){ x - 60.0f, bridgeTop, gap + 120.0f, LEDGE_THICK });
        }

        x += gap;
        if (x >= endX - 40.0f) break;

        float segment = RandRange(&rnd, 150.0f, 380.0f);
        if (x + segment > endX) segment = endX - x;
        if (segment < 60.0f) break;

        top += RandRange(&rnd, -26.0f, 26.0f);
        if (top < GROUND_TOP_HIGH) top = GROUND_TOP_HIGH;
        if (top > GROUND_TOP_LOW)  top = GROUND_TOP_LOW;

        if (!Add(out, (Rectangle){ x, top, segment, GROUND_DEPTH })) break;

        if (RandNext(&rnd) < 0.45f) AddTree(out, &rnd, x + segment * 0.5f, top);

        if (RandNext(&rnd) < 0.30f)
        {
            float blockH = RandRange(&rnd, StepUp() * 0.55f, StepUp() * 0.9f);
            Add(out, (Rectangle){ x + segment * 0.4f, top - blockH,
                                  RandRange(&rnd, 50.0f, 90.0f), blockH });
        }

        if (RandNext(&rnd) < 0.5f)
        {
            LedgeChain(out, &rnd, x + RandRange(&rnd, 20.0f, segment * 0.6f), top, 2 + (int)(RandNext(&rnd) * 3.0f));
        }

        x += segment;
    }

    /* Bridge whatever is left to the right-hand edge slab. */
    if (endX - x > StepAcross() * 0.9f)
    {
        Add(out, (Rectangle){ x + 20.0f, rightTop - StepUp() * 0.8f,
                              endX - x - 40.0f, LEDGE_THICK });
    }
}

void WorldBuildChunk(int index, Chunk *out)
{
    for (unsigned int attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
    {
        BuildOnce(index, attempt * 7919u, out);

        if (WorldChunkTraversable(out))
        {
            out->active = true;
            return;
        }
    }

    /* Guaranteed-crossable fallback: flat ground across the whole chunk. */
    out->index = index;
    out->solidCount = 0;
    out->treeCount = 0;

    float x0 = (float)index * CHUNK_WIDTH;
    float top = WorldEdgeHeight(index);

    Add(out, (Rectangle){ x0, top, CHUNK_EDGE_WIDTH, GROUND_DEPTH });
    Add(out, (Rectangle){ x0 + CHUNK_WIDTH - CHUNK_EDGE_WIDTH, WorldEdgeHeight(index + 1),
                          CHUNK_EDGE_WIDTH, GROUND_DEPTH });
    Add(out, (Rectangle){ x0, top, CHUNK_WIDTH, GROUND_DEPTH });

    out->active = true;
}

Vector2 WorldSpawnPoint(void)
{
    return (Vector2){ 90.0f, WorldEdgeHeight(0) };
}
