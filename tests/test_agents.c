/* The lifecycle, not the behaviour. What is worth pinning here is that
   nothing gets left out: a creature module that is registered but never
   ticked, or never reset between runs, does not fail to build - it fails
   quietly, as an animal that never appears or never goes away. */

#include "tests.h"

#include "entity/agents.h"
#include "entity/aquatic.h"
#include "entity/cat.h"
#include "entity/creatures.h"
#include "entity/rat.h"
#include "entity/species.h"
#include "entity/stalker.h"
#include "entity/vitals.h"
#include "world/season.h"
#include "world/terrain.h"
#include "world/weather.h"
#include "world/worldgen.h"

#include <stdio.h>
#include <string.h>

#define TICK (1.0f / 60.0f)

static void Prepare(void)
{
    SeasonInit();
    WeatherInit(3u);
    WorldSetSeed(20260901u);
    VitalsReset();
    AgentsReset();

    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);
}

static void TestEveryModuleIsWiredUp(void)
{
    puts("agents");

    int n = AgentsModuleCount();

    printf("    %d creature module(s):", n);
    for (int i = 0; i < n; i++) printf(" %s", AgentsModuleName(i));
    printf("\n");

    Check("there are creature modules", n > 0, true);

    int named = 0;
    for (int i = 0; i < n; i++)
    {
        const char *name = AgentsModuleName(i);
        if (name && name[0] && strcmp(name, "?") != 0) named++;
    }

    Check("and all of them are named", named == n, true);
    Check("an index off the end is not", strcmp(AgentsModuleName(n), "?") == 0, true);
}

static void TestOneCallTicksAllOfThem(void)
{
    /* Every registered module should be populating the world off the
       same call. If one were left out of AgentsFixedUpdate it would
       simply never appear, and nothing would say so. */
    Prepare();

    for (int i = 0; i < 60 * 300; i++)
    {
        CreaturesBeginTick();
        CatFixedUpdate(TICK);
        AgentsFixedUpdate(TICK);
    }

    printf("    after five minutes: %d rats, %d in the water, %d in the census\n",
           RatCount(), AquaticCount(), CreaturesCount());

    Check("the land filled up", RatCount() > 0, true);
    Check("and so did the sea", AquaticCount() > 0, true);

    /* Everything alive is in the census, plus the cat. */
    Check("and all of it reached the census",
          CreaturesCount() >= RatCount() + AquaticCount(), true);
    Check("the cat included", CreaturesCountOf(SPECIES_CAT) == 1, true);
}

static void TestResetEmptiesTheWorld(void)
{
    /* A module missing from AgentsReset leaves last run's animals
       standing in the new one. */
    AgentsReset();

    Check("resetting clears the land", RatCount() == 0, true);
    Check("and the sea", AquaticCount() == 0, true);
    Check("and the stalkers", StalkerCount() == 0, true);

    /* The census is double-buffered, so it takes two empty ticks to go
       quiet - and it must actually go quiet. */
    CreaturesBeginTick();
    CreaturesBeginTick();

    Check("and the census with them", CreaturesCount() == 0, true);
}

static void TestResetKeepsTheWorldEdible(void)
{
    /* AgentsReset clears the census, which is also where each module
       registers what to call when one of its animals is eaten. Clearing
       it after the modules have registered would throw those away, and
       nothing would be catchable for the rest of the run - a silent
       failure that only shows up as the cat starving. */
    Prepare();

    for (int i = 0; i < 60 * 60; i++)
    {
        CreaturesBeginTick();
        CatFixedUpdate(TICK);
        AgentsFixedUpdate(TICK);
    }

    int target = -1;
    for (int i = 0; i < RAT_MAX; i++)
    {
        if (RatActive(i)) { target = i; break; }
    }

    Check("there is a rat after a reset and a minute", target >= 0, true);
    if (target < 0) return;

    CatSpawn(RatPosition(target));

    CreaturesBeginTick();
    CatFixedUpdate(TICK);
    AgentsFixedUpdate(TICK);
    CreaturesBeginTick();

    Rectangle box = CatBounds();
    Vector2 mouth = { box.x + box.width * 0.5f, box.y + box.height * 0.5f };

    const Creature *caught = CreaturesCatchable(SPECIES_CAT, mouth);

    Check("and it can still be caught", caught != NULL, true);
    if (!caught) return;

    int before = RatCount();

    Check("and eaten", CreaturesConsume(caught), true);
    Check("which takes it out of the world", RatCount() == before - 1, true);
}

void SuiteAgents(void)
{
    TestEveryModuleIsWiredUp();
    TestOneCallTicksAllOfThem();
    TestResetEmptiesTheWorld();
    TestResetKeepsTheWorldEdible();
}
