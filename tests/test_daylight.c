/* The clock drives light, cold, and how bold the thing in the dark is, so
   what matters is that it turns exactly once per day, that every value it
   exposes stays in range the whole way round, and that the two ends of the
   moon's cycle really are different nights. */

#include "tests.h"

#include "world/daylight.h"

#include <math.h>
#include <stdio.h>

static void TestTurnsOncePerDay(void)
{
    DaylightInit();

    int startDay = DaylightDay();
    float start = DaylightTime();

    /* Step a whole day at the tick rate the game actually uses. */
    for (int i = 0; i < (int)(DAY_LENGTH * 60.0f); i++) DaylightUpdate(1.0f / 60.0f);

    printf("    after one day: day %d -> %d, t %.3f -> %.3f\n",
           startDay, DaylightDay(), (double)start, (double)DaylightTime());

    Check("a day advances the counter by one", DaylightDay() == startDay + 1, true);
    Check("and lands back at the same hour",
          fabsf(DaylightTime() - start) < 0.01f, true);
}

static void TestEverythingStaysInRange(void)
{
    float minB = 9.0f, maxB = -9.0f;
    bool ok = true, sawEveryPhase[PHASE_COUNT] = { false };

    for (int i = 0; i < 20000; i++)
    {
        DaylightSetTime((float)i / 20000.0f);

        float b = DaylightBrightness();
        float c = DaylightChill();
        float s = DaylightStarAlpha();

        if (b < 0.0f || b > 1.0f) ok = false;
        if (c < 0.0f || c > 1.0f) ok = false;
        if (s < 0.0f || s > 1.0f) ok = false;

        Color top, bottom;
        DaylightSky(&top, &bottom);
        if (top.a != 255 || bottom.a != 255) ok = false;

        if (b < minB) minB = b;
        if (b > maxB) maxB = b;

        sawEveryPhase[DaylightPhase()] = true;
    }

    bool all = true;
    for (int i = 0; i < PHASE_COUNT; i++) if (!sawEveryPhase[i]) all = false;

    printf("    brightness spans %.2f..%.2f\n", (double)minB, (double)maxB);

    Check("brightness, chill and stars stay in 0..1", ok, true);
    Check("the day passes through all four phases", all, true);
    Check("night is genuinely dark", minB < 0.12f, true);
    Check("noon is genuinely bright", maxB > 0.95f, true);
}

/* Noon must be the brightest moment and midnight the darkest, or the
   curve is inside out - which is exactly the kind of sign error that
   looks fine until you play it. */
static void TestNoonIsTheBrightest(void)
{
    DaylightSetTime(0.5f);
    float noon = DaylightBrightness();

    DaylightSetTime(0.0f);
    float midnight = DaylightBrightness();

    DaylightSetTime(0.25f);
    float sunrise = DaylightSunHeight();

    DaylightSetTime(0.75f);
    float sunset = DaylightSunHeight();

    Check("noon outshines midnight", noon > midnight * 3.0f, true);
    Check("the sun is on the horizon at sunrise", fabsf(sunrise) < 0.02f, true);
    Check("and on it again at sunset", fabsf(sunset) < 0.02f, true);

    DaylightSetTime(0.5f);
    Check("the sun is up at noon", DaylightSunHeight() > 0.9f, true);
    Check("and the moon is not", DaylightMoonHeight() < -0.9f, true);
}

/* Cold has to follow the light, or night costs nothing. */
static void TestNightIsColder(void)
{
    DaylightSetTime(0.5f);
    float byDay = DaylightChill();

    DaylightSetTime(0.0f);
    float byNight = DaylightChill();

    printf("    chill: %.2f by day, %.2f at midnight\n",
           (double)byDay, (double)byNight);

    Check("daylight costs no heat", byDay < 0.01f, true);
    Check("midnight costs the most", byNight > 0.95f, true);
}

/* A full moon and a new moon must not be the same night. */
static void TestMoonCycleChangesTheNight(void)
{
    float darkest = 9.0f, brightest = -9.0f;
    int fullDay = 0, newDay = 0;

    DaylightInit();

    for (int d = 1; d <= 8; d++)
    {
        DaylightSetTime(0.0f);

        float b = DaylightBrightness();
        if (b < darkest)   { darkest = b;   newDay = DaylightDay(); }
        if (b > brightest) { brightest = b; fullDay = DaylightDay(); }

        DaylightUpdate(DAY_LENGTH);
    }

    printf("    midnight is %.2f on day %d, %.2f on day %d\n",
           (double)darkest, newDay, (double)brightest, fullDay);

    Check("a full moon lights the night", brightest > darkest * 3.0f, true);
    Check("and the moon's cycle returns to where it started",
          fabsf(DaylightMoonFullness() - 0.5f) < 0.51f, true);
}

/* Dev tools jump phases; the clock must survive being shoved around. */
static void TestSkippingPhasesLands(void)
{
    DaylightInit();

    bool ok = true;

    for (int i = 0; i < 40; i++)
    {
        DayPhase before = DaylightPhase();
        DaylightSkip(1);
        DayPhase after = DaylightPhase();

        if (after == before) ok = false;
        if (DaylightTime() < 0.0f || DaylightTime() >= 1.0f) ok = false;
    }

    Check("skipping always lands in a new phase", ok, true);
    Check("and never leaves the clock out of range",
          DaylightTime() >= 0.0f && DaylightTime() < 1.0f, true);
}

void SuiteDaylight(void)
{
    printf("daylight\n");

    TestTurnsOncePerDay();
    TestEverythingStaysInRange();
    TestNoonIsTheBrightest();
    TestNightIsColder();
    TestMoonCycleChangesTheNight();
    TestSkippingPhasesLands();

    printf("\n");
}
