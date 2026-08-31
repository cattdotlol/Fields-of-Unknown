#include "world/worldgen.h"
#include "entity/cat.h"
#include "world/weather.h"

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

static bool RectsOverlap(Rectangle a, Rectangle b)
{
    return !(a.x + a.width <= b.x || b.x + b.width <= a.x ||
             a.y + a.height <= b.y || b.y + b.height <= a.y);
}

/* Ledges and blocks are placed opportunistically, so they have to check
   they are not growing through something already there.

   The margin is horizontal only: padding vertically would make a block
   overlap the very ground it is standing on, and every one of them was
   being rejected for resting on a surface. */
static bool AddIfClear(Chunk *c, Rectangle r, float margin)
{
    Rectangle padded = { r.x - margin, r.y, r.width + margin * 2.0f, r.height };

    for (int i = 0; i < c->solidCount; i++)
    {
        if (RectsOverlap(padded, c->solids[i])) return false;
    }

    return Add(c, r);
}

/* Top face of the highest solid standing under x, or -1 if nothing is.
   `minThickness` keeps trees off 16-unit pipes. The inset stops props
   from perching on the very lip of a ledge. */
static float SupportTopAt(const Chunk *c, float x, float minThickness)
{
    float best = -1.0f;

    for (int i = 0; i < c->solidCount; i++)
    {
        Rectangle r = c->solids[i];

        if (r.height < minThickness) continue;
        if (x < r.x + 8.0f || x > r.x + r.width - 8.0f) continue;

        if (best < 0.0f || r.y < best) best = r.y;
    }

    return best;
}

/* Mushrooms cluster: finding one should suggest looking around. The
   position is snapped to whatever is actually underneath, and skipped
   entirely if that is nothing - which is how they used to end up
   standing on open water. */
static void AddMushrooms(Chunk *c, Rand *rnd, float x, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (c->mushroomCount >= CHUNK_MAX_MUSHROOMS) return;

        float mx = x + RandRange(rnd, -46.0f, 46.0f);
        float support = SupportTopAt(c, mx, 40.0f);

        if (support < 0.0f) continue;

        Mushroom *m = &c->mushrooms[c->mushroomCount++];

        m->x = mx;
        m->baseY = support;
        m->variant = (unsigned int)(RandNext(rnd) * 4096.0f);

        /* The nourishing ones are common, the poisonous ones are not -
           but not so rare that you never meet one. */
        float roll = RandNext(rnd);
        if      (roll < 0.30f) m->species = 0;
        else if (roll < 0.52f) m->species = 1;
        else if (roll < 0.68f) m->species = 3;
        else if (roll < 0.80f) m->species = 2;
        else if (roll < 0.92f) m->species = 5;
        else                   m->species = 4;
    }
}

/* Trees want proper ground, not a pipe. */
static void AddTree(Chunk *c, Rand *rnd, float x)
{
    if (c->treeCount >= CHUNK_MAX_TREES) return;

    float support = SupportTopAt(c, x, 100.0f);
    if (support < 0.0f) return;

    Tree *t = &c->trees[c->treeCount++];

    t->x = x;
    t->baseY = support;
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
        if (!AddIfClear(c, (Rectangle){ x, top, width, LEDGE_THICK }, 6.0f)) return;

        x += RandRange(rnd, width * 0.55f, width + StepAcross() * 0.55f);
    }
}

typedef struct Seg {
    float x, w, top;
} Seg;

static void BuildOnce(int index, unsigned int salt, Chunk *out)
{
    out->index = index;
    out->solidCount = 0;
    out->treeCount = 0;
    out->mushroomCount = 0;

    float x0 = (float)index * CHUNK_WIDTH;
    float endX = x0 + CHUNK_WIDTH - CHUNK_EDGE_WIDTH;
    float leftTop  = WorldEdgeHeight(index);
    float rightTop = WorldEdgeHeight(index + 1);

    Rand rnd = RandSeed(sSeed ^ salt, (unsigned int)(index * 2654435761u));

    Seg segs[20];
    int segCount = 0;

    /* solids[0] and [1] are the edge slabs: traversal starts and ends
       there, and neighbours butt against them. */
    Add(out, (Rectangle){ x0, leftTop, CHUNK_EDGE_WIDTH, GROUND_DEPTH });
    Add(out, (Rectangle){ endX, rightTop, CHUNK_EDGE_WIDTH, GROUND_DEPTH });

    segs[segCount++] = (Seg){ x0, CHUNK_EDGE_WIDTH, leftTop };
    segs[segCount++] = (Seg){ endX, CHUNK_EDGE_WIDTH, rightTop };

    /* --- ground -------------------------------------------------------
       Segment first, flush against the left edge slab, so a seam is
       always somewhere to land rather than a hole to fall through. */
    float x = x0 + CHUNK_EDGE_WIDTH;
    float top = leftTop;

    while (x < endX - 110.0f && segCount < 18)
    {
        float w = RandRange(&rnd, 200.0f, 420.0f);
        if (x + w > endX) w = endX - x;
        if (w < 110.0f) break;

        top += RandRange(&rnd, -26.0f, 26.0f);
        if (top < GROUND_TOP_HIGH) top = GROUND_TOP_HIGH;
        if (top > GROUND_TOP_LOW)  top = GROUND_TOP_LOW;

        if (!Add(out, (Rectangle){ x, top, w, GROUND_DEPTH })) break;
        segs[segCount++] = (Seg){ x, w, top };

        x += w;
        if (x >= endX - 110.0f) break;

        bool wide = (RandNext(&rnd) < 0.24f);
        float gap = wide ? RandRange(&rnd, 170.0f, 250.0f)
                         : RandRange(&rnd, 70.0f, StepAcross() * 0.92f);

        /* Do not open a channel we cannot then close before the edge. */
        if (x + gap > endX - 110.0f) gap = RandRange(&rnd, 70.0f, 110.0f);

        if (wide)
        {
            AddIfClear(out, (Rectangle){ x - 50.0f, top - StepUp() * 0.85f,
                                         gap + 100.0f, LEDGE_THICK }, 6.0f);
        }

        x += gap;
    }

    if (endX - x > StepAcross() * 0.9f)
    {
        AddIfClear(out, (Rectangle){ x + 16.0f, rightTop - StepUp() * 0.8f,
                                     endX - x - 32.0f, LEDGE_THICK }, 6.0f);
    }

    /* --- the high route, and things to hop onto ---------------------- */
    for (int i = 2; i < segCount; i++)
    {
        if (RandNext(&rnd) > 0.50f) continue;

        LedgeChain(out, &rnd, segs[i].x + RandRange(&rnd, 20.0f, segs[i].w * 0.6f),
                   segs[i].top, 2 + (int)(RandNext(&rnd) * 3.0f));
    }

    for (int i = 2; i < segCount; i++)
    {
        if (RandNext(&rnd) > 0.30f) continue;

        float h = RandRange(&rnd, StepUp() * 0.55f, StepUp() * 0.9f);

        AddIfClear(out, (Rectangle){ segs[i].x + segs[i].w * 0.4f, segs[i].top - h,
                                     RandRange(&rnd, 50.0f, 90.0f), h }, 6.0f);
    }

    /* Every chunk needs somewhere above the flood line, or a wet season
       turns it into a wall. If nothing landed, insist on one. */
    bool hasLedge = false;
    for (int i = 0; i < out->solidCount; i++)
    {
        if (out->solids[i].height <= LEDGE_THICK + 1.0f) { hasLedge = true; break; }
    }

    if (!hasLedge)
    {
        for (int i = 2; i < segCount && !hasLedge; i++)
        {
            LedgeChain(out, &rnd, segs[i].x + segs[i].w * 0.3f, segs[i].top, 3);

            for (int k = 0; k < out->solidCount; k++)
            {
                if (out->solids[k].height <= LEDGE_THICK + 1.0f) { hasLedge = true; break; }
            }
        }
    }

    /* --- props, last ---------------------------------------------------
       Only once every solid exists can a prop know what is under it. */
    for (int i = 0; i < segCount; i++)
    {
        float centre = segs[i].x + segs[i].w * 0.5f;
        float spread = segs[i].w * 0.28f;

        if (RandNext(&rnd) < 0.50f)
        {
            AddTree(out, &rnd, centre + RandRange(&rnd, -spread, spread));
        }

        if (RandNext(&rnd) < 0.50f)
        {
            AddMushrooms(out, &rnd, centre + RandRange(&rnd, -spread, spread),
                         1 + (int)(RandNext(&rnd) * 3.0f));
        }
    }
}

bool WorldChunkValid(const Chunk *chunk)
{
    for (int a = 0; a < chunk->solidCount; a++)
    {
        for (int b = a + 1; b < chunk->solidCount; b++)
        {
            if (RectsOverlap(chunk->solids[a], chunk->solids[b])) return false;
        }
    }

    for (int i = 0; i < chunk->mushroomCount; i++)
    {
        if (SupportTopAt(chunk, chunk->mushrooms[i].x, 40.0f) != chunk->mushrooms[i].baseY)
        {
            return false;
        }
    }

    for (int i = 0; i < chunk->treeCount; i++)
    {
        if (SupportTopAt(chunk, chunk->trees[i].x, 100.0f) != chunk->trees[i].baseY)
        {
            return false;
        }
    }

    /* Somewhere to stand when the flood is at its worst. Without this a
       wet season turns the odd chunk into a wall. */
    float dryLine = WeatherMaxWaterY() - 4.0f;
    bool hasDryGround = false;

    for (int i = 0; i < chunk->solidCount; i++)
    {
        if (chunk->solids[i].y < dryLine) { hasDryGround = true; break; }
    }

    if (!hasDryGround) return false;

    return WorldChunkTraversable(chunk);
}

void WorldBuildChunk(int index, Chunk *out)
{
    for (unsigned int attempt = 0; attempt < MAX_ATTEMPTS; attempt++)
    {
        BuildOnce(index, attempt * 7919u, out);

        if (WorldChunkValid(out))
        {
            out->active = true;
            return;
        }
    }

    /* Guaranteed-crossable fallback: flat ground across the whole chunk. */
    out->index = index;
    out->solidCount = 0;
    out->treeCount = 0;
    out->mushroomCount = 0;

    float x0 = (float)index * CHUNK_WIDTH;
    float top = WorldEdgeHeight(index);

    Add(out, (Rectangle){ x0, top, CHUNK_EDGE_WIDTH, GROUND_DEPTH });
    Add(out, (Rectangle){ x0 + CHUNK_WIDTH - CHUNK_EDGE_WIDTH, WorldEdgeHeight(index + 1),
                          CHUNK_EDGE_WIDTH, GROUND_DEPTH });

    /* Fills the middle only - spanning the whole chunk would lay this
       straight through both edge slabs. */
    Add(out, (Rectangle){ x0 + CHUNK_EDGE_WIDTH, top,
                          CHUNK_WIDTH - CHUNK_EDGE_WIDTH * 2.0f, GROUND_DEPTH });

    out->active = true;
}

Vector2 WorldSpawnPoint(void)
{
    return (Vector2){ 90.0f, WorldEdgeHeight(0) };
}
