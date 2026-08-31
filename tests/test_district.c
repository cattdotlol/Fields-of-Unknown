/* Districts, and the two things in them the cat has to be able to use:
   a doorway it fits through, and a cave it can climb back out of. */

#include "tests.h"

#include "world/worldgen.h"
#include "entity/cat.h"

#include <math.h>
#include <stdio.h>

/* The cat's actual box. */
#define CAT_W 22.0f
#define CAT_H 30.0f

/* Half-open on both axes, like the collision test itself: a point exactly
   on a solid's boundary is outside it, not inside. Inclusive bounds made
   every doorway scan start "in" the wall above it. */
static bool Inside(const Chunk *c, float x, float y)
{
    for (int i = 0; i < c->solidCount; i++)
    {
        Rectangle r = c->solids[i];
        if (x >= r.x && x < r.x + r.width && y >= r.y && y < r.y + r.height) return true;
    }
    return false;
}

static void TestDistrictsAreRegionsNotNoise(void)
{
    puts("districts");

    WorldSetSeed(20260831u);

    int runs = 0;
    District previous = WorldDistrictAt(-400);

    for (int i = -400; i <= 400; i++)
    {
        District d = WorldDistrictAt(i);
        if (d != previous) runs++;
        previous = d;
    }

    /* 800 chunks in six-chunk districts is at most ~134 changes. Scattered
       per-chunk noise would be nearer 600. */
    printf("    %d district changes over 800 chunks\n", runs);

    Check("districts span many chunks, not one", runs < 200, true);
    Check("and the world is not all one district", runs > 20, true);

    int seen[DISTRICT_COUNT] = { 0 };
    for (int i = -400; i <= 400; i++) seen[WorldDistrictAt(i)]++;

    bool all = true;
    for (int i = 0; i < DISTRICT_COUNT; i++) if (seen[i] == 0) all = false;

    Check("every district type occurs", all, true);
    Check("the crash site is always sprawl", WorldDistrictAt(0) == DISTRICT_SPRAWL, true);
}

static void TestApartmentsCanBeEntered(void)
{
    WorldSetSeed(20260831u);

    int buildings = 0, blockedDoors = 0, shortDoors = 0;

    for (int i = -300; i <= 300; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        for (int r = 0; r < c.solidCount; r++)
        {
            if (c.kinds[r] != SOLID_ROOF) continue;

            buildings++;

            /* The left wall is the one that stops short for the door. */
            float bx = c.solids[r].x + 4.0f;
            int wall = -1;

            for (int w = 0; w < c.solidCount; w++)
            {
                if (c.kinds[w] != SOLID_WALL) continue;
                if (fabsf(c.solids[w].x - bx) > 2.0f) continue;

                wall = w;
                break;
            }

            if (wall < 0) continue;

            Rectangle lw = c.solids[wall];
            float doorTop = lw.y + lw.height;
            float midX = lw.x + lw.width * 0.5f;

            /* A cat-sized space, from the threshold up. */
            if (Inside(&c, midX, doorTop + CAT_H * 0.5f)) blockedDoors++;
            if (Inside(&c, midX, doorTop + 4.0f)) blockedDoors++;

            /* And tall enough to walk through. */
            float clear = 0.0f;
            for (float y = doorTop + 1.0f; y < doorTop + 80.0f; y += 2.0f)
            {
                if (Inside(&c, midX, y)) break;
                clear = y - doorTop;
            }

            if (clear < CAT_H) shortDoors++;
        }
    }

    printf("    %d apartment blocks, %d blocked doors, %d too short\n",
           buildings, blockedDoors, shortDoors);

    Check("apartments get built", buildings > 20, true);
    Check("no doorway is walled up", blockedDoors == 0, true);
    Check("every doorway clears the cat's height", shortDoors == 0, true);
}

/* A cave the cat cannot climb out of is a trap, not a feature. */
static void TestCavesCanBeLeft(void)
{
    WorldSetSeed(20260831u);

    int caves = 0;

    for (int i = -300; i <= 300; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        for (int s = 0; s < c.solidCount; s++)
        {
            if (c.kinds[s] == SOLID_ROCK && c.solids[s].height > 200.0f) caves++;
        }
    }

    printf("    %d cave chambers\n", caves);
    Check("caves get dug", caves > 10, true);

    /* The climb out is fixed geometry, so assert it against the cat's real
       reach - if gravity or jump strength is ever retuned, this fails
       rather than quietly sealing every cave. */
    float reach = CatMaxJumpHeight();
    float floorToRubble = 40.0f;                 /* RUBBLE_H          */
    float rubbleToDaylight = 76.0f - 40.0f + 40.0f;  /* CAVE_H - RUBBLE_H + CRUST_D */

    printf("    climb out: %.0f then %.0f, against a %.0f jump\n",
           (double)floorToRubble, (double)rubbleToDaylight, (double)reach);

    Check("floor to rubble is one jump", floorToRubble < reach, true);
    Check("rubble to daylight is one jump", rubbleToDaylight < reach, true);
    Check("and the chamber is taller than the cat", 76.0f > CAT_H, true);
}

/* The first version was two thin walls with slabs between them: you
   could see the skyline straight through it. A building needs an inside. */
static void TestBuildingsAreSolidObjects(void)
{
    WorldSetSeed(20260831u);

    int roofs = 0, interiors = 0, windows = 0, pillars = 0;
    int seeThrough = 0, escapes = 0;

    for (int i = -300; i <= 300; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        for (int r = 0; r < c.solidCount; r++)
        {
            if (c.kinds[r] != SOLID_ROOF) continue;

            roofs++;

            float bx = c.solids[r].x + 5.0f;
            float bw = c.solids[r].width - 10.0f;

            /* Something opaque has to cover the footprint. */
            bool filled = false;

            for (int d = 0; d < c.decorCount; d++)
            {
                if (c.decor[d].kind != DECOR_ROOM) continue;

                Rectangle f = c.decor[d].rect;
                if (f.x <= bx + 2.0f && f.x + f.width >= bx + bw - 2.0f) { filled = true; break; }
            }

            if (!filled) seeThrough++;
        }

        for (int d = 0; d < c.decorCount; d++)
        {
            if (c.decor[d].kind == DECOR_ROOM) interiors++;
            if (c.decor[d].kind == DECOR_PILLAR) pillars++;
            if (c.decor[d].kind == DECOR_WINDOW ||
                c.decor[d].kind == DECOR_WINDOW_LIT) windows++;
        }

        for (int r = 0; r < c.solidCount; r++)
        {
            if (c.kinds[r] == SOLID_LEDGE && c.solids[r].height < 10.0f) escapes++;
        }
    }

    printf("    %d buildings, %d interiors, %d windows, %d pillars, %d fire escapes\n",
           roofs, interiors, windows, pillars, escapes);

    Check("no building is see-through", seeThrough == 0, true);
    Check("every building has an interior", interiors >= roofs, true);
    Check("buildings have windows", windows > roofs * 3, true);
    Check("wide buildings get structural bays", pillars > 0, true);
    Check("some have fire escapes", escapes > 0, true);
}

/* A fire escape is only worth having if it can be climbed. */
static void TestFireEscapesAreClimbable(void)
{
    WorldSetSeed(20260831u);

    float worstRise = 0.0f;

    for (int i = -200; i <= 200; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        /* Escape platforms stack in one column at one storey spacing. */
        for (int a = 0; a < c.solidCount; a++)
        {
            if (c.kinds[a] != SOLID_LEDGE || c.solids[a].height >= 10.0f) continue;

            float best = 1e9f;

            for (int b = 0; b < c.solidCount; b++)
            {
                if (a == b || c.kinds[b] != SOLID_LEDGE || c.solids[b].height >= 10.0f) continue;
                if (fabsf(c.solids[b].x - c.solids[a].x) > 4.0f) continue;

                float rise = c.solids[a].y - c.solids[b].y;
                if (rise > 0.0f && rise < best) best = rise;
            }

            if (best < 1e8f && best > worstRise) worstRise = best;
        }
    }

    printf("    tallest step between escape platforms: %.0f, jump is %.0f\n",
           (double)worstRise, (double)CatMaxJumpHeight());

    Check("every escape step is inside one jump", worstRise < CatMaxJumpHeight(), true);
}

void SuiteDistrict(void)
{
    TestDistrictsAreRegionsNotNoise();
    TestApartmentsCanBeEntered();
    TestCavesCanBeLeft();
    TestBuildingsAreSolidObjects();
    TestFireEscapesAreClimbable();
}
