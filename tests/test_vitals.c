/* Vitals and seasons are pure tuning: a sign error or a rate off by a
   factor of ten is invisible until someone plays for ten minutes. */

#include "tests.h"

#include "entity/vitals.h"
#include "entity/cat.h"
#include "world/season.h"
#include "world/daylight.h"
#include "world/terrain.h"
#include "world/weather.h"

#include <stdio.h>
#include <string.h>

#define TICK (1.0f / 60.0f)

/* Runs the sim forward and reports how long until `stop` becomes true. */
static float SecondsUntil(bool (*stop)(void), float limit)
{
    for (float t = 0.0f; t < limit; t += TICK)
    {
        SeasonUpdate(TICK);
        WeatherUpdate(TICK);
        VitalsUpdate(TICK);

        if (stop()) return t;
    }

    return -1.0f;
}

static bool Dead(void)     { return gVitals.dead; }

static void TestHungerIsTheClock(void)
{
    puts("vitals");

    SeasonInit();
    WeatherInit(1u);
    VitalsReset();
    CatSpawn((Vector2){ 0.0f, 0.0f });

    /* Warmth pinned, so this measures hunger and nothing else. */
    float t = -1.0f;
    for (float e = 0.0f; e < 3600.0f; e += TICK)
    {
        gVitals.warmth = 1.0f;
        SeasonUpdate(TICK);
        WeatherUpdate(TICK);
        VitalsUpdate(TICK);

        if (gVitals.hunger <= 0.0f) { t = e; break; }
    }

    printf("    idle from 85%% full to starving: %.0fs\n", (double)t);

    Check("starving takes longer than five minutes", t > 300.0f, true);
    Check("and lands inside half an hour", t > 0.0f && t < 1800.0f, true);
}

/* Hunger is meant to be the thing that kills you. Standing in ordinary
   weather should not. */
static void TestMildWeatherIsSurvivable(void)
{
    SeasonInit();
    WeatherInit(1u);
    VitalsReset();

    float lowest = 1.0f;

    for (int i = 0; i < 60 * 600; i++)      /* ten minutes idle */
    {
        SeasonUpdate(TICK);
        WeatherUpdate(TICK);
        VitalsUpdate(TICK);

        if (gVitals.warmth < lowest) lowest = gVitals.warmth;
    }

    printf("    ten idle minutes: warmth bottomed at %.2f, health %.2f\n",
           (double)lowest, (double)gVitals.health);

    Check("standing in the rain does not freeze the cat", gVitals.dead, false);
    Check("warmth finds an equilibrium above zero", lowest > 0.05f, true);
}

static void TestStarvingKills(void)
{
    SeasonInit();
    WeatherInit(1u);
    VitalsReset();

    gVitals.hunger = 0.0f;

    float t = SecondsUntil(Dead, 600.0f);
    printf("    starving to death: %.0fs\n", (double)t);

    Check("empty stomach eventually kills", t > 0.0f, true);
    Check("but not instantly", t > 20.0f, true);
}

static void TestFedAndWarmRecovers(void)
{
    SeasonInit();
    WeatherInit(1u);
    VitalsReset();

    gVitals.health = 0.4f;
    gVitals.hunger = 0.9f;
    gVitals.warmth = 0.9f;

    for (int i = 0; i < 600; i++) VitalsUpdate(TICK);

    Check("health regenerates when fed and warm", gVitals.health > 0.4f, true);
}

static void TestEverythingStaysInRange(void)
{
    SeasonInit();
    WeatherInit(7u);
    VitalsReset();

    bool inRange = true;

    for (int i = 0; i < 60 * 600; i++)   /* ten minutes */
    {
        SeasonUpdate(TICK);
        WeatherUpdate(TICK);
        VitalsUpdate(TICK);

        if (gVitals.health  < 0.0f || gVitals.health  > 1.0f) inRange = false;
        if (gVitals.hunger  < 0.0f || gVitals.hunger  > 1.0f) inRange = false;
        if (gVitals.stamina < 0.0f || gVitals.stamina > 1.0f) inRange = false;
        if (gVitals.warmth  < 0.0f || gVitals.warmth  > 1.0f) inRange = false;
    }

    Check("no vital ever leaves 0..1", inRange, true);
}

static void TestStamina(void)
{
    VitalsReset();

    VitalsSpendStamina(0.5f);
    Check("spending stamina costs it", gVitals.stamina < 0.55f, true);

    VitalsSpendStamina(10.0f);
    Check("stamina cannot go negative", gVitals.stamina == 0.0f, true);
    Check("empty stamina refuses a sprint", VitalsHasStamina(0.02f), false);
}

static void TestSeasonsTurn(void)
{
    puts("seasons");

    SeasonInit();

    bool seen[SEASON_COUNT] = { false };
    float minTemp = 2.0f, maxTemp = -1.0f;
    float lastTemp = SeasonTemperature();
    float biggestJump = 0.0f;

    /* One full year at one second a step. */
    for (int i = 0; i < 1300; i++)
    {
        SeasonUpdate(1.0f);

        seen[SeasonCurrent()] = true;

        float temp = SeasonTemperature();
        if (temp < minTemp) minTemp = temp;
        if (temp > maxTemp) maxTemp = temp;

        float jump = temp - lastTemp;
        if (jump < 0.0f) jump = -jump;
        if (jump > biggestJump) biggestJump = jump;
        lastTemp = temp;
    }

    bool all = true;
    for (int i = 0; i < SEASON_COUNT; i++) if (!seen[i]) all = false;

    printf("    temperature %.2f..%.2f, biggest one-second jump %.4f\n",
           (double)minTemp, (double)maxTemp, (double)biggestJump);

    Check("a year visits all four seasons", all, true);
    Check("temperature stays within 0..1", minTemp >= 0.0f && maxTemp <= 1.0f, true);
    Check("temperature never snaps between frames", biggestJump < 0.02f, true);
}

/* Weather must come from WORLD_SEED alone. It used to share raylib's
   global generator with the film grain, which draws hundreds of numbers
   a frame - so the sky depended on whether the intro was on screen. */
static void TestWeatherIsReproducible(void)
{
    puts("weather");

    char first[32], second[32];

    for (int pass = 0; pass < 2; pass++)
    {
        SeasonInit();
        WeatherInit(20260831u);

        char *out = (pass == 0) ? first : second;
        int n = 0;

        for (int f = 0; f < 60 * 400 && n < 31; f++)
        {
            /* Second pass churns the global RNG the way drawing does. */
            if (pass == 1)
            {
                for (int g = 0; g < 300; g++) GetRandomValue(0, 1000);
            }

            SeasonUpdate(TICK);
            WeatherUpdate(TICK);

            /* The state index, not the name's first letter: DRY and
               DRIZZLE both begin with D, so a name fingerprint cannot
               tell them apart and the test would pass on anything. */
            if (f % (60 * 20) == 0) out[n++] = (char)('0' + (int)WeatherCurrent());
        }

        out[n] = '\0';
    }

    printf("    %s vs %s\n", first, second);

    Check("the same seed gives the same weather",
          strcmp(first, second) == 0, true);

    /* And that the fingerprint is worth comparing at all. */
    bool varies = false;
    for (int i = 1; first[i] != '\0'; i++) if (first[i] != first[0]) varies = true;

    Check("the weather actually changes over the run", varies, true);
}

/* Two tunings that were wrong in ways only arithmetic catches. */
static void TestMovementTuning(void)
{
    puts("movement");

    /* The bob is driven by distance travelled, so its frequency scales
       with speed. At the old rate a sprint bobbed 20 times a second. */
    float sprintHz = CatRunSpeed() * CatStrideRate();

    printf("    gait at a sprint: %.1f cycles/second\n", (double)sprintHz);

    Check("a sprint does not flicker", sprintHz < 6.0f, true);
    Check("but the legs do move", sprintHz > 1.5f, true);

    /* Buoyancy parks the cat at a fixed depth. If the kick window is
       shallower than that, it can never fire and water is a trap. */
    float rest = CatSwimRestDepth();
    float window = CatSwimKickWindow();

    printf("    buoyancy rests at %.1f deep, kick reaches %.0f\n",
           (double)rest, (double)window);

    Check("the cat can push off from where it floats", rest < window, true);
    Check("with room to spare", window - rest > 5.0f, true);
}

/* Wind used to be multiplied by rainfall, so it only existed in storms
   and everything that reacts to it stood still on a clear day. */
static void TestWindMoves(void)
{
    puts("wind");

    SeasonInit();
    WeatherInit(11u);

    float lowest = 2.0f, highest = -2.0f;
    float previous = WeatherWind();
    float sharpest = 0.0f;
    int reversals = 0;

    for (int i = 0; i < 60 * 240; i++)      /* four minutes */
    {
        SeasonUpdate(TICK);
        WeatherUpdate(TICK);

        float w = WeatherWind();

        if (w < lowest) lowest = w;
        if (w > highest) highest = w;

        float step = w - previous;
        if (step < 0.0f) step = -step;
        if (step > sharpest) sharpest = step;

        if ((w > 0.0f) != (previous > 0.0f)) reversals++;
        previous = w;
    }

    printf("    range %.2f..%.2f, %d direction changes, sharpest step %.4f\n",
           (double)lowest, (double)highest, reversals, (double)sharpest);

    Check("the wind actually moves", highest - lowest > 0.2f, true);
    Check("it blows both ways", lowest < 0.0f && highest > 0.0f, true);
    Check("and stays inside its range", lowest >= -1.0f && highest <= 1.0f, true);
    Check("without jumping between frames", sharpest < 0.05f, true);
}


/* --- nights ------------------------------------------------------------
   Night is the game's only real teacher: nothing tells the cat to get
   under something, so the cost of not doing it has to be legible in the
   numbers. These pin the shape - a summer night out is survivable, a
   winter one is not, and a roof changes both. */

static bool FindSpot(bool roofed, Vector2 *out)
{
    for (float x = -6000.0f; x < 6000.0f; x += 7.0f)
    {
        TerrainStream(x);

        for (float y = 200.0f; y < 620.0f; y += 7.0f)
        {
            Vector2 p = { x, y };
            Rectangle body = { x - 9.0f, y - 18.0f, 18.0f, 18.0f };

            if (TerrainOverlaps(body)) continue;

            float c = TerrainCoverAbove(p);
            if (roofed ? (c > 0.92f) : (c < 0.02f)) { *out = p; return true; }
        }
    }
    return false;
}

/* Warmth left at first light, or -1 if the cat did not see it. */
static float NightOut(Season season, Vector2 where)
{
    SeasonSet(season);
    WeatherForceState(WEATHER_DRY);
    VitalsReset();
    DaylightInit();
    DaylightSetTime(0.78f);              /* nightfall */
    CatSpawn(where);
    TerrainStream(where.x);

    int ticks = (int)(0.52f * DAY_LENGTH * 60.0f);

    for (int i = 0; i < ticks; i++)
    {
        VitalsUpdate(1.0f / 60.0f);
        DaylightUpdate(1.0f / 60.0f);
    }

    return gVitals.dead ? -1.0f : gVitals.warmth;
}

static void TestNightsCostSomething(void)
{
    Vector2 open, roofed;

    if (!FindSpot(false, &open) || !FindSpot(true, &roofed))
    {
        Check("found somewhere open and somewhere roofed", false, true);
        return;
    }

    float summerOut = NightOut(SEASON_SUMMER, open);
    float autumnOut = NightOut(SEASON_AUTUMN, open);
    float winterOut = NightOut(SEASON_WINTER, open);
    float winterIn  = NightOut(SEASON_WINTER, roofed);

    printf("    a night out: summer %.2f, autumn %.2f, winter %s"
           " (winter under a roof %.2f)\n",
           (double)summerOut, (double)autumnOut,
           winterOut < 0.0f ? "fatal" : "survived", (double)winterIn);

    Check("a summer night out is survivable", summerOut > 0.35f, true);
    Check("but it still costs heat", summerOut < 0.75f, true);
    Check("an autumn night out nearly finishes the cat",
          autumnOut > 0.0f && autumnOut < 0.30f, true);
    Check("a winter night in the open kills", winterOut < 0.0f, true);
    Check("the same night under a roof does not", winterIn > 0.5f, true);
}

static void TestShelterIsWorthFinding(void)
{
    Vector2 open, roofed;

    if (!FindSpot(false, &open) || !FindSpot(true, &roofed))
    {
        Check("found somewhere open and somewhere roofed", false, true);
        return;
    }

    float out = NightOut(SEASON_SPRING, open);
    float in  = NightOut(SEASON_SPRING, roofed);

    printf("    spring night: %.2f in the open, %.2f under a roof\n",
           (double)out, (double)in);

    Check("a roof is plainly better than none", in > out + 0.3f, true);
}

/* Daylight must not cost heat, or the cat freezes at noon. */
static void TestDaytimeIsFree(void)
{
    Vector2 open;
    if (!FindSpot(false, &open)) { Check("found open ground", false, true); return; }

    SeasonSet(SEASON_SPRING);
    WeatherForceState(WEATHER_DRY);
    VitalsReset();
    DaylightInit();
    DaylightSetTime(0.5f);               /* noon */
    CatSpawn(open);
    TerrainStream(open.x);

    float before = gVitals.warmth;
    for (int i = 0; i < 60 * 60; i++) VitalsUpdate(1.0f / 60.0f);

    printf("    an hour at noon: warmth %.2f -> %.2f\n",
           (double)before, (double)gVitals.warmth);

    Check("noon does not chill the cat", gVitals.warmth >= before, true);
}

void SuiteVitals(void)
{
    TestHungerIsTheClock();
    TestMildWeatherIsSurvivable();
    TestStarvingKills();
    TestFedAndWarmRecovers();
    TestEverythingStaysInRange();
    TestStamina();
    TestSeasonsTurn();
    TestWeatherIsReproducible();
    TestMovementTuning();
    TestWindMoves();

    TestNightsCostSomething();
    TestShelterIsWorthFinding();
    TestDaytimeIsFree();

    /* These leave the world at a hard hour in a hard season. Put it back,
       or every suite after this one runs at midnight in winter. */
    DaylightInit();
    SeasonSet(SEASON_SPRING);
    WeatherForceState(WEATHER_DRY);
}
