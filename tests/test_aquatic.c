/* What lives in the flood, and where. The migration is the part that is
   invisible in play - it happens over an hour and you only ever see one
   slice of it - so the whole point of these is to look at the slices
   side by side and check the layer really moves. */

#include "tests.h"

#include "entity/aquatic.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "world/daylight.h"
#include "world/ocean.h"
#include "world/season.h"
#include "world/terrain.h"
#include "world/weather.h"
#include "world/worldgen.h"

#include <stdio.h>

#define TICK (1.0f / 60.0f)

static void Prepare(float hour)
{
    SeasonInit();
    WeatherInit(5u);
    WorldSetSeed(20260901u);
    DaylightInit();
    DaylightSetTime(hour);
    VitalsReset();
    AquaticReset();

    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);
}

/* Populates the sea and lets it settle at whatever hour it is. */
static void Settle(int seconds)
{
    for (int i = 0; i < 60 * seconds; i++) AquaticFixedUpdate(TICK);
}

static float MeanDepth(AquaticKind kind)
{
    float sum = 0.0f;
    int n = 0;

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (!AquaticActive(i) || AquaticKindOf(i) != kind) continue;

        sum += OceanDepthAt(AquaticPosition(i).y);
        n++;
    }

    return n ? (sum / (float)n) : -1.0f;
}

static void TestTheSeaFillsUp(void)
{
    puts("aquatic");

    Prepare(0.5f);
    Settle(400);

    printf("    %d jellyfish, %d shark(s), %d whale\n",
           AquaticCountOf(AQUA_JELLY), AquaticCountOf(AQUA_SHARK),
           AquaticCountOf(AQUA_WHALE));

    Check("jellyfish turn up", AquaticCountOf(AQUA_JELLY) > 0, true);
    Check("so do sharks", AquaticCountOf(AQUA_SHARK) > 0, true);
    Check("and never more than the cap", AquaticCount() <= AQUATIC_MAX, true);
}

static void TestNothingLivesInTheRubble(void)
{
    /* The drowned city is a lid: the top three hundred of the water
       column is nearly solid. Anything spawned up there is inside a
       building, where it will sit wedged for the rest of its life. */
    Prepare(0.0f);
    Settle(400);

    int stuck = 0, shallowest = 100000;

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (!AquaticActive(i)) continue;

        float d = OceanDepthAt(AquaticPosition(i).y);
        if (d < (float)shallowest) shallowest = (int)d;

        Vector2 p = AquaticPosition(i);
        Rectangle box = { p.x - 8.0f, p.y - 8.0f, 16.0f, 16.0f };

        if (TerrainOverlaps(box)) stuck++;
    }

    printf("    at midnight, the shallowest is %d down; %d inside rock\n",
           shallowest, stuck);

    Check("nothing is embedded in the ruins", stuck == 0, true);
    Check("and nothing swims above the drowned city", shallowest > 240, true);
}

static void TestTheLayerRises(void)
{
    /* Diel vertical migration: deep by day, shallow after dark. */
    Prepare(0.5f);
    Settle(400);
    float jellyDay = MeanDepth(AQUA_JELLY);
    float sharkDay = MeanDepth(AQUA_SHARK);

    Prepare(0.0f);
    Settle(400);
    float jellyNight = MeanDepth(AQUA_JELLY);
    float sharkNight = MeanDepth(AQUA_SHARK);

    printf("    jellyfish: %.0f down at noon, %.0f at midnight\n",
           (double)jellyDay, (double)jellyNight);
    printf("    sharks:    %.0f down at noon, %.0f at midnight\n",
           (double)sharkDay, (double)sharkNight);

    Check("jellyfish come up after dark", jellyNight < jellyDay, true);
    Check("and it is a move worth noticing", jellyDay - jellyNight > 200.0f, true);
    Check("sharks follow them up", sharkNight < sharkDay, true);
}

static void TestTheGlowGoesWhereTheDarkIs(void)
{
    /* Bioluminescence is worth nothing where there is daylight left to
       drown it out. Read off one individual twice without moving it or
       advancing its pulse, so the only thing that differs between the
       two numbers is the sky. */
    Prepare(0.0f);
    AquaticForceSpawn(AQUA_JELLY, CatPosition().x + 400.0f);

    Check("a jellyfish glows", AquaticGlow(0) > 0.0f, true);
    Check("an index with nothing in it does not", AquaticGlow(-1) <= 0.0f, true);

    float night = AquaticGlow(0);

    DaylightSetTime(0.5f);
    float noon = AquaticGlow(0);

    printf("    one jellyfish %.0f down: glow %.2f at midnight, %.2f at noon\n",
           (double)OceanDepthAt(AquaticPosition(0).y), (double)night, (double)noon);

    Check("and it is worth more when there is no daylight left",
          night > noon * 1.3f, true);

    /* Nothing else in the water makes its own light. */
    Prepare(0.5f);
    Settle(400);

    bool blank = true;

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (!AquaticActive(i) || AquaticKindOf(i) == AQUA_JELLY) continue;
        if (AquaticGlow(i) > 0.0f) blank = false;
    }

    Check("sharks and whales stay dark", blank, true);
}

static void TestTheWhaleWorksTheWholeColumn(void)
{
    /* It is the only thing with a reason to cross every zone, and the
       only thing that goes below where the cat can follow. */
    Prepare(0.35f);
    AquaticForceSpawn(AQUA_WHALE, CatPosition().x + 500.0f);

    float shallowest = 1e9f, deepest = -1e9f;

    for (int i = 0; i < 60 * 900; i++)
    {
        /* Keep the cat alongside, or the whale leaves the despawn radius
           long before it has finished one dive. */
        CatSpawn((Vector2){ AquaticPosition(0).x, CatPosition().y });
        TerrainStream(CatPosition().x);
        AquaticFixedUpdate(TICK);

        if (!AquaticActive(0)) break;

        float d = OceanDepthAt(AquaticPosition(0).y);
        if (d < shallowest) shallowest = d;
        if (d > deepest) deepest = d;
    }

    printf("    over fifteen minutes it worked %.0f down to %.0f\n",
           (double)shallowest, (double)deepest);

    Check("the whale is still there at the end", AquaticActive(0), true);
    Check("it comes up under the city", shallowest < 420.0f, true);
    Check("it goes down to the plain", deepest > 700.0f, true);
    Check("and it crosses a zone boundary doing it",
          OceanZoneAtDepth(shallowest) != OceanZoneAtDepth(deepest), true);
}

static void TestJellyfishStillCostNothing(void)
{
    /* Sharks spawn on their own and will bite whatever is in the water,
       so a plain health check here measures the wrong animal. Only the
       ticks with nothing else within reach count. */
    Prepare(0.0f);
    AquaticForceSpawn(AQUA_JELLY, CatPosition().x + 400.0f);
    VitalsReset();

    int touching = 0, hurt = 0;

    for (int t = 0; t < 60 * 120; t++)
    {
        if (!AquaticActive(0) || AquaticKindOf(0) != AQUA_JELLY) break;

        Vector2 jelly = AquaticPosition(0);

        CatSpawn(jelly);
        CatFixedUpdate(TICK);

        float before = gVitals.health;
        AquaticFixedUpdate(TICK);

        /* Anything that bites, anywhere near. */
        bool alone = true;

        for (int i = 0; i < AQUATIC_MAX; i++)
        {
            if (!AquaticActive(i) || AquaticKindOf(i) == AQUA_JELLY) continue;

            Vector2 p = AquaticPosition(i);
            float dx = p.x - jelly.x, dy = p.y - jelly.y;

            if (dx * dx + dy * dy < 400.0f * 400.0f) alone = false;
        }

        if (!alone) { gVitals.health = 1.0f; continue; }

        touching++;
        if (gVitals.health < before - 0.0001f) hurt++;
    }

    printf("    %d tick(s) sat inside a jellyfish with nothing else near, "
           "%d of them cost health\n", touching, hurt);

    /* The sting this replaced ran on a 1.4 second cooldown, so three
       clear seconds is already several chances for it to have fired. */
    Check("there was time alone with one to measure", touching > 180, true);
    Check("and swimming through it costs nothing", hurt == 0, true);
}

void SuiteAquatic(void)
{
    TestTheSeaFillsUp();
    TestNothingLivesInTheRubble();
    TestTheLayerRises();
    TestTheGlowGoesWhereTheDarkIs();
    TestTheWhaleWorksTheWholeColumn();
    TestJellyfishStillCostNothing();

    /* This suite leaves the clock wherever the last test put it. */
    DaylightInit();
}
