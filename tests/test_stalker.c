/* The predator is the only thing that can kill the cat directly, so the
   things worth pinning are that it exists, that it keeps its distance
   until it has a reason not to, that it can actually hurt you, and that
   going quiet remains an answer. */

#include "tests.h"

#include "entity/stalker.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "world/terrain.h"
#include "world/worldgen.h"
#include "world/weather.h"
#include "world/season.h"

#include <math.h>
#include <stdio.h>

#define TICK (1.0f / 60.0f)

static void Prepare(void)
{
    SeasonInit();
    WeatherInit(3u);
    WorldSetSeed(20260831u);
    StalkersReset();
    VitalsReset();

    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);
}

static void Run(int ticks)
{
    for (int i = 0; i < ticks; i++) StalkersFixedUpdate(TICK);
}

static void TestGracePeriod(void)
{
    puts("stalker");

    Prepare();
    Run(60 * 20);            /* twenty seconds: inside the grace window */

    Check("nothing appears in the first twenty seconds", StalkerCount() == 0, true);

    Run(60 * 300);           /* five more minutes */

    printf("    %d stalker(s) after five minutes\n", StalkerCount());
    Check("one turns up eventually", StalkerCount() > 0, true);
    Check("never more than the cap", StalkerCount() <= STALKER_MAX, true);
}

static void TestItSpawnsAtADistance(void)
{
    Prepare();

    float closest = 1e9f;

    for (int i = 0; i < 60 * 600; i++)
    {
        StalkersFixedUpdate(TICK);

        for (int s = 0; s < STALKER_MAX; s++)
        {
            if (!StalkerActive(s)) continue;

            /* Only the tick it appeared on matters, so check them all and
               keep the smallest first sighting. */
            float d = fabsf(StalkerPosition(s).x - CatPosition().x);
            if (StalkerCurrentState(s) == STALK_PROWL && d < closest) closest = d;
        }
    }

    printf("    closest a prowling stalker got: %.0f units\n", (double)closest);
    Check("it never materialises on top of the cat", closest > 200.0f, true);
}

/* A quiet cat is not worth crossing the map for. */
static void TestSilenceIsSafety(void)
{
    Prepare();
    Run(60 * 400);

    int subject = -1;
    for (int s = 0; s < STALKER_MAX; s++) if (StalkerActive(s)) { subject = s; break; }
    if (subject < 0) { Check("a stalker to observe", false, true); return; }

    /* The cat has been idle throughout, so its noise is near zero. */
    Run(60 * 60);

    printf("    interest after a minute of an idle cat: %.2f\n",
           (double)StalkerInterest(subject));

    Check("an idle cat does not draw it in", StalkerInterest(subject) < 0.45f, true);
    Check("so it is not hunting", StalkerHunting() == 0, true);
}

static void TestItCanHurtYou(void)
{
    Prepare();
    Run(60 * 400);

    int subject = -1;
    for (int s = 0; s < STALKER_MAX; s++) if (StalkerActive(s)) { subject = s; break; }
    if (subject < 0) { Check("a stalker to be hit by", false, true); return; }

    VitalsReset();
    float before = gVitals.health;

    /* Walk straight into it. */
    CatSpawn(StalkerPosition(subject));
    Run(60 * 3);

    printf("    health %.2f -> %.2f after walking into it\n",
           (double)before, (double)gVitals.health);

    Check("it takes health off you", gVitals.health < before, true);
    Check("but not all of it at once", gVitals.health > 0.4f, true);
}

static void TestItStaysOutOfTheWater(void)
{
    Prepare();
    Run(60 * 500);

    int drowned = 0;
    for (int s = 0; s < STALKER_MAX; s++)
    {
        if (!StalkerActive(s)) continue;
        if (StalkerPosition(s).y > 1200.0f) drowned++;
    }

    Check("none walk off into the water", drowned == 0, true);
}

void SuiteStalker(void)
{
    TestGracePeriod();
    TestItSpawnsAtADistance();
    TestSilenceIsSafety();
    TestItCanHurtYou();
    TestItStaysOutOfTheWater();
}
