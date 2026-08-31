/* Generation is the one system that can silently ship an unplayable
   world, so it gets the tests. Run with: make test */

#include "world/worldgen.h"
#include "world/terrain.h"
#include "entity/cat.h"

#include "tests.h"

#include "raylib.h"

#include <math.h>
#include <stdio.h>

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

/* The three things that were actually wrong: mushrooms standing on open
   water, solids growing through each other, and nothing checking either. */
static void TestNothingFloatsOrOverlaps(void)
{
    puts("placement");

    WorldSetSeed(20260831u);

    int floatingShrooms = 0, floatingTrees = 0, overlaps = 0;
    int shrooms = 0, trees = 0, invalid = 0;

    for (int i = -300; i <= 300; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        if (!WorldChunkValid(&c)) invalid++;

        for (int m = 0; m < c.mushroomCount; m++)
        {
            shrooms++;
            bool held = false;

            for (int sIdx = 0; sIdx < c.solidCount; sIdx++)
            {
                Rectangle r = c.solids[sIdx];
                if (c.mushrooms[m].x >= r.x && c.mushrooms[m].x <= r.x + r.width &&
                    c.mushrooms[m].baseY == r.y) { held = true; break; }
            }
            if (!held) floatingShrooms++;
        }

        for (int t = 0; t < c.treeCount; t++)
        {
            trees++;
            bool held = false;

            for (int sIdx = 0; sIdx < c.solidCount; sIdx++)
            {
                Rectangle r = c.solids[sIdx];
                if (c.trees[t].x >= r.x && c.trees[t].x <= r.x + r.width &&
                    c.trees[t].baseY == r.y) { held = true; break; }
            }
            if (!held) floatingTrees++;
        }

        for (int a = 0; a < c.solidCount; a++)
        {
            for (int b = a + 1; b < c.solidCount; b++)
            {
                Rectangle p = c.solids[a], q = c.solids[b];
                bool hit = !(p.x + p.width <= q.x || q.x + q.width <= p.x ||
                             p.y + p.height <= q.y || q.y + q.height <= p.y);
                if (hit) overlaps++;
            }
        }
    }

    printf("    %d mushrooms, %d trees over 601 chunks\n", shrooms, trees);

    Check("no mushroom stands on open water", floatingShrooms == 0, true);
    Check("no tree stands on open water", floatingTrees == 0, true);
    Check("no solid grows through another", overlaps == 0, true);
    Check("every generated chunk passes validation", invalid == 0, true);
}

/* Validation is only worth having if it actually rejects things. */
static void TestValidationHasTeeth(void)
{
    Chunk c = { 0 };
    c.solidCount = 3;
    c.solids[0] = (Rectangle){    0, 560, 180, 280 };
    c.solids[1] = (Rectangle){  240, 560, 180, 280 };
    c.solids[2] = (Rectangle){   60, 420, 140,  16 };   /* above the flood */

    Check("a clean chunk passes", WorldChunkValid(&c), true);

    /* Nowhere above the waterline: a wet season would wall this off. */
    c.solidCount = 2;
    Check("a chunk with no dry ground is rejected", WorldChunkValid(&c), false);

    c.solidCount = 4;
    c.solids[3] = (Rectangle){  100, 500, 180, 280 };   /* through solid 0 */
    Check("overlapping solids are rejected", WorldChunkValid(&c), false);

    c.solidCount = 3;
    c.mushroomCount = 1;
    c.mushrooms[0] = (Mushroom){ .x = 210.0f, .baseY = 560.0f };  /* in the gap */
    Check("a floating mushroom is rejected", WorldChunkValid(&c), false);

    c.mushrooms[0].x = 300.0f;                                    /* on solid 1 */
    Check("a supported mushroom is accepted", WorldChunkValid(&c), true);
}

void SuiteWorldgen(void)
{
    TestValidatorRejectsImpossible();
    TestEveryChunkIsCrossable();
    TestSeamsLineUp();
    TestDeterminism();
    TestStreaming();
    TestNothingFloatsOrOverlaps();
    TestValidationHasTeeth();
}
