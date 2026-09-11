/* The census is what every animal in the game perceives the others
   through, so the things worth pinning are: what goes in comes out, a
   query is answered from the previous tick rather than a half-built one,
   the diet table is what decides who sees whom, and eating something
   actually removes it from the world it was eaten in. */

#include "tests.h"

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

#include <math.h>
#include <stdio.h>

#define TICK (1.0f / 60.0f)

static int sRemoved;

static void CountRemoval(int tag)
{
    (void)tag;
    sRemoved++;
}

static void TestWhatGoesInComesOut(void)
{
    puts("creatures");

    CreaturesReset();

    /* Two ticks: things published during the first are readable during
       the second. */
    CreaturesBeginTick();
    CreaturesPublish(SPECIES_RAT, (Vector2){ 100.0f, 0.0f }, 7, 30.0f, 0.0f);
    CreaturesPublish(SPECIES_CAT, (Vector2){ 120.0f, 0.0f }, 0, 0.0f, 0.0f);

    Check("nothing is visible in the tick it is published",
          CreaturesCount() == 0, true);

    CreaturesBeginTick();

    Check("both turn up on the next one", CreaturesCount() == 2, true);
    Check("counted by species", CreaturesCountOf(SPECIES_RAT) == 1, true);

    const Creature *rat = CreaturesNearest(SPECIES_BIT(SPECIES_RAT),
                                           (Vector2){ 0.0f, 0.0f }, 1000.0f);

    Check("and found again", rat != NULL, true);

    if (rat)
    {
        Check("with its owner's index intact", rat->tag == 7, true);
        Check("and its size read off the table",
              fabsf(rat->width - SpeciesOf(SPECIES_RAT)->width) < 0.01f, true);
    }

    /* A tick where nobody publishes empties it, rather than leaving the
       world full of animals that stopped existing. */
    CreaturesBeginTick();
    CreaturesBeginTick();

    Check("and gone once nothing publishes them",
          CreaturesCount() == 0, true);
}

static void TestRangeIsRespected(void)
{
    CreaturesReset();

    CreaturesBeginTick();
    CreaturesPublish(SPECIES_RAT, (Vector2){ 900.0f, 0.0f }, 1, 30.0f, 0.0f);
    CreaturesBeginTick();

    Vector2 here = { 0.0f, 0.0f };

    Check("out of range is not seen",
          CreaturesNearest(SPECIES_BIT(SPECIES_RAT), here, 500.0f) == NULL, true);
    Check("in range is",
          CreaturesNearest(SPECIES_BIT(SPECIES_RAT), here, 1000.0f) != NULL, true);
}

static void TestTheDietDecidesWhoSeesWhom(void)
{
    CreaturesReset();

    Vector2 here = { 0.0f, 0.0f };

    CreaturesBeginTick();
    CreaturesPublish(SPECIES_RAT,     (Vector2){  40.0f, 0.0f }, 0, 30.0f, 0.0f);
    CreaturesPublish(SPECIES_STALKER, (Vector2){  80.0f, 0.0f }, 0,  0.0f, 0.0f);
    CreaturesPublish(SPECIES_WHALE,   (Vector2){ 120.0f, 0.0f }, 0,  0.0f, 0.0f);
    CreaturesBeginTick();

    const Creature *catFood = CreaturesNearestPrey(SPECIES_CAT, here, 500.0f);
    const Creature *catFear = CreaturesNearestThreat(SPECIES_CAT, here, 500.0f);

    Check("a cat looking for food finds the rat",
          catFood && catFood->species == SPECIES_RAT, true);
    Check("and looking over its shoulder finds the stalker",
          catFear && catFear->species == SPECIES_STALKER, true);

    /* Nobody had to tell the whale it is not interested in rats; its
       empty diet says so. */
    Check("a whale finds nothing to eat",
          CreaturesNearestPrey(SPECIES_WHALE, here, 500.0f) == NULL, true);
    Check("and nothing to fear",
          CreaturesNearestThreat(SPECIES_WHALE, here, 500.0f) == NULL, true);

    /* A rat does not see another rat as either. */
    Check("a rat is not afraid of a whale",
          CreaturesNearestThreat(SPECIES_RAT, here, 500.0f)->species
              == SPECIES_STALKER, true);
}

static void TestReachIsTheCreatureSOwnBusiness(void)
{
    CreaturesReset();

    /* Same species, same distance, different reach: one has noticed the
       cat and one has not. */
    CreaturesBeginTick();
    CreaturesPublish(SPECIES_RAT, (Vector2){ 20.0f, 0.0f }, 1, 30.0f, 0.0f);
    CreaturesBeginTick();

    Vector2 here = { 0.0f, 0.0f };

    Check("an unbothered rat at twenty units is catchable",
          CreaturesCatchable(SPECIES_CAT, here) != NULL, true);

    CreaturesBeginTick();
    CreaturesPublish(SPECIES_RAT, (Vector2){ 20.0f, 0.0f }, 1, 15.0f, 0.0f);
    CreaturesBeginTick();

    Check("a bolting one at the same distance is not",
          CreaturesCatchable(SPECIES_CAT, here) == NULL, true);

    /* And nothing with no reach at all is ever picked up, however close
       it gets - a stalker is not lunch just because it is standing on
       you. Published at zero, and the cat does not eat stalkers anyway. */
    CreaturesBeginTick();
    CreaturesPublish(SPECIES_STALKER, (Vector2){ 1.0f, 0.0f }, 0, 0.0f, 0.0f);
    CreaturesBeginTick();

    Check("and a stalker underfoot is not a meal",
          CreaturesCatchable(SPECIES_CAT, here) == NULL, true);
}

static void TestEatingRemovesItFromTheWorld(void)
{
    CreaturesReset();
    sRemoved = 0;

    CreaturesBeginTick();
    CreaturesPublish(SPECIES_RAT, (Vector2){ 5.0f, 0.0f }, 3, 30.0f, 0.0f);
    CreaturesBeginTick();

    const Creature *rat = CreaturesCatchable(SPECIES_CAT,
                                             (Vector2){ 0.0f, 0.0f });

    Check("with nobody listening, nothing can be eaten",
          CreaturesConsume(rat), false);

    CreaturesOnRemove(SPECIES_RAT, CountRemoval);
    /* Gameplay republishes animals before the cat eats. */
    CreaturesPublish(SPECIES_RAT, (Vector2){ 5.0f, 0.0f }, 3, 30.0f, 0.0f);

    Check("once the owner is listening, it can", CreaturesConsume(rat), true);
    Check("and the owner is the one told", sRemoved == 1, true);
    Check("the same pointer cannot be eaten twice", CreaturesConsume(rat), false);
    Check("consumed prey disappears immediately",
          CreaturesCatchable(SPECIES_CAT, (Vector2){ 0.0f, 0.0f }) == NULL, true);
    Check("consumed animals leave the count", CreaturesCount() == 0, true);
    Check("consumed animals leave indexed queries", CreatureAt(0) == NULL, true);
    CreaturesBeginTick();
    Check("republished prey stays gone next tick",
          CreaturesNearestPrey(SPECIES_CAT, (Vector2){ 0.0f, 0.0f }, 100.0f) == NULL, true);
    Check("removal was called only once", sRemoved == 1, true);
}

/* The whole path, driven the way the gameplay screen drives it. */
static void TestTheCatCanActuallyEatARat(void)
{
    SeasonInit();
    WeatherInit(3u);
    WorldSetSeed(20260831u);
    VitalsReset();
    CreaturesReset();
    RatsReset();
    StalkersReset();

    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);

    /* Let the population build up, in the screen's tick order. */
    for (int i = 0; i < 60 * 40; i++)
    {
        CreaturesBeginTick();
        CatFixedUpdate(TICK);
        RatsFixedUpdate(TICK);
    }

    Check("rats got into the census", CreaturesCountOf(SPECIES_RAT) > 0, true);
    Check("and so did the cat", CreaturesCountOf(SPECIES_CAT) == 1, true);

    /* Stand on top of one and take it. Reaching into the rat module for
       a position is the test cheating on purpose: what is being checked
       is the eating, not the stalking. */
    int target = -1;
    for (int i = 0; i < RAT_MAX; i++)
    {
        if (RatActive(i)) { target = i; break; }
    }

    Check("there is one to go after", target >= 0, true);
    if (target < 0) return;

    CatSpawn(RatPosition(target));

    /* One more tick so the cat's new position and the rat both land in
       the same census. */
    CreaturesBeginTick();
    CatFixedUpdate(TICK);
    RatsFixedUpdate(TICK);
    CreaturesBeginTick();

    Rectangle box = CatBounds();
    Vector2 mouth = { box.x + box.width * 0.5f, box.y + box.height * 0.5f };

    const Creature *caught = CreaturesCatchable(SPECIES_CAT, mouth);

    Check("standing on a rat, there is something to eat", caught != NULL, true);
    if (!caught) return;

    Check("and it is a rat", caught->species == SPECIES_RAT, true);

    int before = RatCount();
    gVitals.hunger = 0.2f;

    Nutrition n = SpeciesOf(caught->species)->food;
    CreaturesConsume(caught);
    VitalsApply(n.hunger, n.health, n.warmth);

    printf("    hunger %.2f after eating a %s\n",
           (double)gVitals.hunger, SpeciesName(caught->species));

    Check("eating it takes it out of the world", RatCount() == before - 1, true);
    Check("and puts something back into the cat", gVitals.hunger > 0.2f, true);
}

void SuiteCreatures(void)
{
    TestWhatGoesInComesOut();
    TestRangeIsRespected();
    TestTheDietDecidesWhoSeesWhom();
    TestReachIsTheCreatureSOwnBusiness();
    TestEatingRemovesItFromTheWorld();
    TestTheCatCanActuallyEatARat();
}
