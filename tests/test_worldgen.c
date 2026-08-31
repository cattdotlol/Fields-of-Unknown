/* Generation is the one system that can silently ship an unplayable
   world, so it gets the tests. Run with: make test */

#include "world/worldgen.h"
#include "world/terrain.h"
#include "entity/cat.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>

static int sFailures;

static void Check(const char *name, bool got, bool expected)
{
    bool ok = (got == expected);
    if (!ok) sFailures++;

    printf("  %-46s %s\n", name, ok ? "ok" : "FAIL");
}

/* A chunk the cat cannot cross must be rejected, or the validator is
   decoration. */
static void TestValidatorRejectsImpossible(void)
{
    puts("validator");

    Chunk c = { 0 };
    c.solidCount = 2;
    c.solids[0] = (Rectangle){    0, 560, 180, 280 };
    c.solids[1] = (Rectangle){  900, 560, 180, 280 };   /* 720 gap */
    Check("rejects an unjumpable gap", WorldChunkTraversable(&c), false);

    c.solids[1] = (Rectangle){  240, 560, 180, 280 };   /* 60 gap */
    Check("accepts a jumpable gap", WorldChunkTraversable(&c), true);

    c.solids[1] = (Rectangle){  240, 400, 180, 440 };   /* 160 step up */
    Check("rejects a step past jump height", WorldChunkTraversable(&c), false);

    c.solidCount = 1;
    Check("rejects a chunk with no far edge", WorldChunkTraversable(&c), false);
}

static void TestEveryChunkIsCrossable(void)
{
    puts("generation");

    WorldSetSeed(20260831u);

    int bad = 0, trees = 0, maxSolids = 0;

    /* Far from the origin and negative, since the world runs both ways. */
    for (int i = -300; i <= 300; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        if (!WorldChunkTraversable(&c)) { bad++; if (bad == 1) printf("    chunk %d uncrossable\n", i); }
        if (c.solidCount > maxSolids) maxSolids = c.solidCount;
        trees += c.treeCount;
    }

    printf("    601 chunks, %d trees, max %d solids (cap %d)\n",
           trees, maxSolids, CHUNK_MAX_SOLIDS);

    Check("every chunk near the origin is crossable", bad == 0, true);
    Check("chunks carry trees", trees > 0, true);
    Check("no chunk hits the solid cap", maxSolids < CHUNK_MAX_SOLIDS, true);

    /* A long way out, where float precision starts to matter. */
    bad = 0;
    for (int i = 1000000; i < 1000040; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);
        if (!WorldChunkTraversable(&c)) bad++;
    }
    Check("chunks a million out are still crossable", bad == 0, true);
}

/* Neighbours must agree on the ground height where they meet, or the
   world has a cliff or a hole at every seam. */
static void TestSeamsLineUp(void)
{
    puts("seams");

    WorldSetSeed(20260831u);

    int mismatched = 0;

    for (int i = -200; i < 200; i++)
    {
        Chunk left, right;
        WorldBuildChunk(i, &left);
        WorldBuildChunk(i + 1, &right);

        /* solids[1] is the right edge slab, solids[0] the left one. */
        Rectangle a = left.solids[1];
        Rectangle b = right.solids[0];

        if (fabsf(a.y - b.y) > 0.001f) mismatched++;
        if (fabsf((a.x + a.width) - b.x) > 0.001f) mismatched++;
    }

    Check("neighbouring chunks share edge height and x", mismatched == 0, true);
}

static void TestDeterminism(void)
{
    puts("determinism");

    WorldSetSeed(4242u);

    Chunk first;
    WorldBuildChunk(77, &first);

    /* Build other chunks in between: generation must not depend on order. */
    Chunk scratch;
    for (int i = 0; i < 20; i++) WorldBuildChunk(i, &scratch);

    Chunk again;
    WorldBuildChunk(77, &again);

    bool same = (first.solidCount == again.solidCount) &&
                (first.treeCount == again.treeCount);

    for (int i = 0; same && i < first.solidCount; i++)
    {
        same = (first.solids[i].x == again.solids[i].x &&
                first.solids[i].y == again.solids[i].y &&
                first.solids[i].width == again.solids[i].width);
    }

    Check("a chunk rebuilds identically, in any order", same, true);

    WorldSetSeed(999u);
    Chunk other;
    WorldBuildChunk(77, &other);
    Check("a different seed gives a different chunk",
          other.solidCount != first.solidCount ||
          other.solids[2].x != first.solids[2].x, true);
}

/* Walking far in one direction must keep ground under the cat and must
   not grow memory: the window is fixed size. */
static void TestStreaming(void)
{
    puts("streaming");

    WorldSetSeed(20260831u);

    int overWindow = 0;
    float run = 0.0f, worstGap = 0.0f;

    /* Gaps are deliberate - they are the water channels. What matters is
       that no gap is wider than the cat can cross. */
    for (float x = 0.0f; x < 120000.0f; x += 6.0f)
    {
        TerrainStream(x);

        if (TerrainLoadedChunks() > TERRAIN_LOADED_CHUNKS) overWindow++;

        bool ground = false;
        for (int i = 0; i < TerrainCount(); i++)
        {
            Rectangle r = TerrainSolid(i);
            if (x >= r.x && x <= r.x + r.width) { ground = true; break; }
        }

        if (ground) run = 0.0f;
        else
        {
            run += 6.0f;
            if (run > worstGap) worstGap = run;
        }
    }

    printf("    widest gap %.0f units, run jump %.0f\n",
           (double)worstGap, (double)CatMaxRunJumpDistance());

    Check("no gap is wider than the cat can jump",
          worstGap < CatMaxRunJumpDistance(), true);
    Check("the loaded window never grows", overWindow == 0, true);

    /* And back the other way, to the same place. */
    TerrainStream(0.0f);
    int a = TerrainCount();
    TerrainStream(90000.0f);
    TerrainStream(0.0f);
    Check("returning to the origin restores the same chunks",
          TerrainCount() == a, true);
}

int main(void)
{
    SetTraceLogLevel(LOG_ERROR);

    printf("cat reach: %.1f up, %.1f across\n\n",
           (double)CatMaxJumpHeight(), (double)CatMaxRunJumpDistance());

    TestValidatorRejectsImpossible();
    TestEveryChunkIsCrossable();
    TestSeamsLineUp();
    TestDeterminism();
    TestStreaming();

    printf("\n%s\n", sFailures ? "FAILED" : "all passed");
    return sFailures ? 1 : 0;
}
