/* Vitals and seasons are pure tuning: a sign error or a rate off by a
   factor of ten is invisible until someone plays for ten minutes. */

#include "tests.h"

#include "entity/vitals.h"
#include "entity/cat.h"
#include "world/season.h"
#include "world/weather.h"

#include <stdio.h>

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

void SuiteVitals(void)
{
    TestHungerIsTheClock();
    TestMildWeatherIsSurvivable();
    TestStarvingKills();
    TestFedAndWarmRecovers();
    TestEverythingStaysInRange();
    TestStamina();
    TestSeasonsTurn();
}
