/* Mushrooms are the only thing that feeds the cat, and the harvest table
   is the one piece of mutable world state - both worth pinning down. */

#include "tests.h"

#include "world/mushroom.h"
#include "world/worldgen.h"
#include "entity/vitals.h"

#include <stdio.h>

static void TestChunksGrowThem(void)
{
    puts("mushrooms");

    WorldSetSeed(20260831u);

    int total = 0, chunksWith = 0;
    int perSpecies[MUSHROOM_SPECIES] = { 0 };
    int overCap = 0;

    for (int i = -200; i <= 200; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        if (c.mushroomCount > CHUNK_MAX_MUSHROOMS) overCap++;
        if (c.mushroomCount > 0) chunksWith++;
        total += c.mushroomCount;

        for (int m = 0; m < c.mushroomCount; m++)
        {
            if (c.mushrooms[m].species < MUSHROOM_SPECIES) perSpecies[c.mushrooms[m].species]++;
        }
    }

    printf("    %d mushrooms over 401 chunks, %d chunks have some\n", total, chunksWith);
    printf("    species spread:");
    for (int i = 0; i < MUSHROOM_SPECIES; i++) printf(" %d", perSpecies[i]);
    printf("\n");

    Check("mushrooms actually grow", total > 100, true);
    Check("but not in every chunk", chunksWith < 401, true);
    Check("never over the per-chunk cap", overCap == 0, true);

    bool allSeen = true;
    for (int i = 0; i < MUSHROOM_SPECIES; i++) if (perSpecies[i] == 0) allSeen = false;
    Check("every species turns up somewhere", allSeen, true);
}

static void TestSomeFeedAndSomePoison(void)
{
    int good = 0, bad = 0;

    for (unsigned char i = 0; i < MUSHROOM_SPECIES; i++)
    {
        MushroomEffect e = MushroomEffectOf(i);

        if (e.hunger > 0.0f && e.health >= 0.0f) good++;
        if (e.health < 0.0f) bad++;
    }

    Check("some species are food", good >= 3, true);
    Check("some species are poison", bad >= 1, true);

    /* Nothing should be able to kill outright from full health. */
    bool survivable = true;
    for (unsigned char i = 0; i < MUSHROOM_SPECIES; i++)
    {
        if (MushroomEffectOf(i).health <= -1.0f) survivable = false;
    }
    Check("no single mushroom is instantly fatal", survivable, true);
}

static void TestEatingWorks(void)
{
    VitalsReset();
    gVitals.hunger = 0.30f;

    MushroomEffect e = MushroomEffectOf(0);   /* pale cap, plain food */
    VitalsApply(e.hunger, e.health, e.warmth);

    Check("eating raises hunger", gVitals.hunger > 0.30f, true);

    VitalsReset();
    float before = gVitals.health;

    MushroomEffect bad = MushroomEffectOf(5); /* grey slime */
    VitalsApply(bad.hunger, bad.health, bad.warmth);

    Check("poison costs health", gVitals.health < before, true);
}

static void TestHarvestRegrows(void)
{
    MushroomClearHarvests();

    Check("nothing is harvested to begin with", MushroomIsHarvested(3, 1), false);

    MushroomHarvest(3, 1);
    Check("harvesting is remembered", MushroomIsHarvested(3, 1), true);
    Check("and does not affect its neighbour", MushroomIsHarvested(3, 2), false);

    MushroomTick(120.0f);
    Check("still gone after two minutes", MushroomIsHarvested(3, 1), true);

    MushroomTick(200.0f);
    Check("grown back after five", MushroomIsHarvested(3, 1), false);
}

static void TestHarvestTableIsBounded(void)
{
    MushroomClearHarvests();

    /* Far more than the table holds: it must forget, not overflow. */
    for (int i = 0; i < 5000; i++) MushroomHarvest(i, i & 7);

    Check("the newest harvest is remembered", MushroomIsHarvested(4999, 4999 & 7), true);
    Check("the oldest has been forgotten", MushroomIsHarvested(0, 0), false);
}

void SuiteMushroom(void)
{
    TestChunksGrowThem();
    TestSomeFeedAndSomePoison();
    TestEatingWorks();
    TestHarvestRegrows();
    TestHarvestTableIsBounded();
}
