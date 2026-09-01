/* What lives in the flood, and where. The migration is the part that is
   invisible in play - it happens over an hour and you only ever see one
   slice of it - so the whole point of these is to look at the slices
   side by side and check the layer really moves. */

#include "tests.h"

#include "entity/aquatic.h"
#include "core/input.h"
#include "entity/creatures.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "world/daylight.h"
#include "world/ocean.h"
#include "world/season.h"
#include "world/terrain.h"
#include "world/weather.h"
#include "world/worldgen.h"

#include <math.h>
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
    CreaturesReset();
    AquaticReset();

    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);
}

/* Populates the sea and lets it settle at whatever hour it is. Drives
   the census the way the gameplay screen does, or nothing down there can
   see anything else and half the behaviour never runs. */
static void Settle(int seconds)
{
    for (int i = 0; i < 60 * seconds; i++)
    {
        CreaturesBeginTick();
        CatFixedUpdate(TICK);
        AquaticFixedUpdate(TICK);
    }
}

/* Mean distance from each fish to the nearest other fish. A school is
   tight; a sprinkle is not. */
static float SchoolTightness(void)
{
    float sum = 0.0f;
    int n = 0;

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (!AquaticActive(i) || AquaticKindOf(i) != AQUA_FISH) continue;

        float best = 1e9f;

        for (int j = 0; j < AQUATIC_MAX; j++)
        {
            if (j == i || !AquaticActive(j) || AquaticKindOf(j) != AQUA_FISH) continue;

            Vector2 a = AquaticPosition(i), b = AquaticPosition(j);
            float dx = a.x - b.x, dy = a.y - b.y;
            float d = sqrtf(dx * dx + dy * dy);

            if (d < best) best = d;
        }

        if (best < 1e8f) { sum += best; n++; }
    }

    return n ? sum / (float)n : -1.0f;
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

    printf("    %d jellyfish, %d shark(s), %d whale, %d fish\n",
           AquaticCountOf(AQUA_JELLY), AquaticCountOf(AQUA_SHARK),
           AquaticCountOf(AQUA_WHALE), AquaticCountOf(AQUA_FISH));

    Check("jellyfish turn up", AquaticCountOf(AQUA_JELLY) > 0, true);
    Check("and so do fish", AquaticCountOf(AQUA_FISH) > 0, true);
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

        /* Anything that bites, anywhere near. Asked of the food web
           rather than listed here, so an animal added later is counted
           without anyone remembering to come back and add it. */
        bool alone = true;

        for (int i = 0; i < AQUATIC_MAX; i++)
        {
            if (!AquaticActive(i)) continue;
            if (!SpeciesEats(AquaticSpeciesOf(i), SPECIES_CAT)) continue;

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

static void TestFishSchool(void)
{
    Prepare(0.5f);
    Settle(400);

    float gap = SchoolTightness();

    printf("    %d fish, nearest neighbour %.0f away on average\n",
           AquaticCountOf(AQUA_FISH), (double)gap);

    Check("there is a school", AquaticCountOf(AQUA_FISH) > 4, true);

    /* They spawn in a cluster, so this only means anything if it is
       still true after four hundred seconds of swimming. Loose enough
       not to be a heap, tight enough not to be a sprinkle. */
    Check("and it holds together", gap > 0.0f && gap < 60.0f, true);
}

/* How much of the school is calm enough to be caught right now. */
static float CalmFraction(void)
{
    int calm = 0, fish = 0;

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (!AquaticActive(i) || AquaticKindOf(i) != AQUA_FISH) continue;

        fish++;
        if (AquaticAlarm(i) < 0.4f) calm++;
    }

    return fish ? (float)calm / (float)fish : -1.0f;
}

static void TestTheSchoolIsLeftAloneToBeASchool(void)
{
    /* A shark that never stops hunting lives inside the school, which
       holds every fish in it at full alarm forever - and a permanently
       alarmed fish cannot be caught by anything slower than a shark,
       the cat included. Sharks feeding and then leaving is what gives
       the water its quiet.

       Sampled over two minutes rather than at one instant: whether a
       shark happens to be in the school at any given second is luck,
       and the claim being made is that the quiet exists at all. */
    Prepare(0.5f);
    Settle(400);

    float best = 0.0f;
    int quietSeconds = 0;

    for (int sec = 0; sec < 120; sec++)
    {
        Settle(1);

        float calm = CalmFraction();
        if (calm > best) best = calm;
        if (calm > 0.5f) quietSeconds++;
    }

    printf("    over two minutes: at best %.0f%% of the school was calm, "
           "and it was mostly calm for %ds\n",
           (double)(best * 100.0f), quietSeconds);

    Check("the school gets left alone", best > 0.5f, true);
    Check("and stays that way for a while", quietSeconds > 20, true);
}

static void TestSharksLiveOnFishRatherThanOnYou(void)
{
    /* The cat never enters the water in this one. Anything the school
       loses, it loses to the sharks. */
    Prepare(0.5f);
    Settle(400);

    int start = AquaticCountOf(AQUA_FISH);
    int low = start;

    for (int t = 0; t < 60 * 600; t++)
    {
        CreaturesBeginTick();
        CatFixedUpdate(TICK);
        AquaticFixedUpdate(TICK);

        int now = AquaticCountOf(AQUA_FISH);
        if (now < low) low = now;
    }

    int end = AquaticCountOf(AQUA_FISH);

    printf("    ten minutes later: %d fish, having dipped to %d (from %d)\n",
           end, low, start);

    /* Something ate some of them. */
    Check("the sharks take fish", low < start, true);

    /* But not all of them: a predator that empties the sea is a bug,
       not a food chain. */
    Check("and the sea does not end up empty", end > 0, true);
}

/* The index of a fish that has not noticed anything, letting the sea run
   until one turns up. A player waits for this too. */
static int CalmFish(int patienceSeconds)
{
    for (int sec = 0; sec < patienceSeconds; sec++)
    {
        for (int i = 0; i < AQUATIC_MAX; i++)
        {
            if (!AquaticActive(i) || AquaticKindOf(i) != AQUA_FISH) continue;
            if (AquaticAlarm(i) < 0.15f) return i;
        }

        Settle(1);
    }

    return -1;
}

/* Holds the cat on top of a fish for a while and counts the ticks it
   could have taken it. `working` decides the only thing that differs
   between the two approaches: whether the cat is swimming at the fish or
   letting itself drift onto it. */
static int TicksCatchable(int fish, int seconds, bool working)
{
    int got = 0;

    InputScriptBegin();
    InputScriptHold(ACT_RIGHT, working);
    InputScriptHold(ACT_DOWN, working);

    for (int t = 0; t < 60 * seconds; t++)
    {
        if (!AquaticActive(fish) || AquaticKindOf(fish) != AQUA_FISH) break;

        /* Pinned in place, so distance is never what decides it. */
        CatSpawn(AquaticPosition(fish));

        InputPoll();

        CreaturesBeginTick();
        CatFixedUpdate(TICK);
        AquaticFixedUpdate(TICK);

        Rectangle box = CatBounds();
        Vector2 mouth = { box.x + box.width * 0.5f, box.y + box.height * 0.5f };

        const Creature *c = CreaturesCatchable(SPECIES_CAT, mouth);
        if (c && c->species == SPECIES_FISH) got++;
    }

    InputScriptEnd();

    return got;
}

static void TestAQuietCatCanEatAndALoudOneCannot(void)
{
    /* The whole reason to dive. Noise is the currency on land - a rat
       hears you coming - and it is the currency down here too, except
       that the cat is a poor swimmer, so easing in is the only approach
       that works at all. */
    Prepare(0.5f);
    Settle(400);

    int f = CalmFish(90);

    Check("a fish that has not noticed anything turns up", f >= 0, true);
    if (f < 0) return;

    int quiet = TicksCatchable(f, 2, false);

    printf("    drifting onto one: catchable on %d of 120 ticks\n", quiet);
    Check("a cat that eases in gets hold of one", quiet > 0, true);

    /* Same fish, held at the same distance: the only thing that differs
       is whether the cat is working the water or letting it carry it. */
    f = CalmFish(90);
    if (f < 0) return;

    int loud = TicksCatchable(f, 2, true);

    printf("    and thrashing at one: %d\n", loud);
    Check("and one that thrashes loses it", loud < quiet, true);
}

void SuiteAquatic(void)
{
    TestTheSeaFillsUp();
    TestNothingLivesInTheRubble();
    TestTheLayerRises();
    TestTheGlowGoesWhereTheDarkIs();
    TestTheWhaleWorksTheWholeColumn();
    TestJellyfishStillCostNothing();
    TestFishSchool();
    TestTheSchoolIsLeftAloneToBeASchool();
    TestSharksLiveOnFishRatherThanOnYou();
    TestAQuietCatCanEatAndALoudOneCannot();

    /* This suite leaves the clock wherever the last test put it. */
    DaylightInit();
}
