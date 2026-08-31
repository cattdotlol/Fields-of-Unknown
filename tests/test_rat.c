/* Rats are the first thing in the world that acts on its own, so the
   things worth pinning are: they stand on ground, they stay in bounds,
   they notice the cat, and they can be caught. */

#include "tests.h"

#include "entity/rat.h"
#include "entity/cat.h"
#include "world/terrain.h"
#include "world/worldgen.h"
#include "world/weather.h"
#include "world/season.h"

#include <math.h>
#include <stdio.h>

#define TICK (1.0f / 60.0f)

static void Settle(int ticks)
{
    for (int i = 0; i < ticks; i++) RatsFixedUpdate(TICK);
}

static void Prepare(void)
{
    SeasonInit();
    WeatherInit(3u);
    WorldSetSeed(20260831u);
    RatsReset();

    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);
}

static void TestTheyAppearAndStandOnSomething(void)
{
    puts("rats");

    Prepare();
    Settle(60 * 40);        /* forty seconds */

    int count = RatCount();
    printf("    %d rats after forty seconds\n", count);

    Check("rats turn up", count > 0, true);
    Check("but never more than the pool", count <= RAT_MAX, true);
    Check("the population reaches its target", count >= RAT_TARGET, true);

    /* None should have fallen through the world. */
    int fallen = 0;
    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!RatActive(i)) continue;
        if (RatPosition(i).y > 1200.0f) fallen++;
    }

    Check("none fall out of the world", fallen == 0, true);
}

static void TestTheyStayNearTheCat(void)
{
    Prepare();
    Settle(60 * 30);

    int strays = 0;
    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!RatActive(i)) continue;
        if (fabsf(RatPosition(i).x - CatPosition().x) > 3000.0f) strays++;
    }

    Check("none linger far from the cat", strays == 0, true);
}

/* Something almost on top of a rat registers no matter how quiet it is. */
static void TestTheyNoticeTheCat(void)
{
    Prepare();
    Settle(60 * 40);

    int subject = -1;
    for (int i = 0; i < RAT_MAX; i++) if (RatActive(i)) { subject = i; break; }

    Check("there is a rat to test with", subject >= 0, true);
    if (subject < 0) return;

    /* Stand right on it. */
    CatSpawn(RatPosition(subject));
    TerrainStream(CatPosition().x);

    /* Watch the whole reaction, not the state afterwards: a rat bolts
       within a second and has calmed down again a couple of seconds
       later, so sampling only at the end sees nothing happen. */
    bool bolted = false;
    float peakAlert = 0.0f;

    for (int i = 0; i < 60 * 3; i++)
    {
        RatsFixedUpdate(TICK);

        if (RatAlarmed() > 0) bolted = true;
        if (RatAlertLevel(subject) > peakAlert) peakAlert = RatAlertLevel(subject);
    }

    printf("    stood on a rat: peak alert %.2f, bolted %s\n",
           (double)peakAlert, bolted ? "yes" : "no");

    Check("being stood on makes a rat bolt", bolted, true);
    Check("its alert passed the panic threshold", peakAlert > 0.7f, true);

    /* And it settles once the cat is no longer on top of it. */
    Vector2 rp = RatPosition(subject);
    Check("it ran away from the cat", fabsf(rp.x - CatPosition().x) > 60.0f, true);
}

static void TestCatching(void)
{
    Prepare();
    Settle(60 * 40);

    int subject = -1;
    for (int i = 0; i < RAT_MAX; i++) if (RatActive(i)) { subject = i; break; }
    if (subject < 0) { Check("there is a rat to catch", false, true); return; }

    Vector2 far = { RatPosition(subject).x + 900.0f, RatPosition(subject).y };
    Rectangle away = { far.x, far.y - 30.0f, 22.0f, 30.0f };

    Check("nothing is catchable from across the map", RatCatchable(away) >= 0, false);

    Vector2 p = RatPosition(subject);
    Rectangle onTop = { p.x - 11.0f, p.y - 30.0f, 22.0f, 30.0f };

    int caught = RatCatchable(onTop);
    Check("a rat underfoot is catchable", caught >= 0, true);

    if (caught >= 0)
    {
        int before = RatCount();
        RatConsume(caught);

        Check("eating one removes it", RatCount() == before - 1, true);
        Check("and it is no longer catchable", RatCatchable(onTop) == caught, false);
    }
}

void SuiteRat(void)
{
    TestTheyAppearAndStandOnSomething();
    TestTheyStayNearTheCat();
    TestTheyNoticeTheCat();
    TestCatching();
}
