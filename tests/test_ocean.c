/* The ocean is three curves and a floor, and everything else reads them:
   lighting dims by OceanLight, vitals freeze by OceanChill and crush past
   OCEAN_CRUSH_DEPTH, and everything that swims steers by OceanFloorAt.
   None of that is visible from inside those callers, so it is pinned
   here instead. */

#include "tests.h"

#include "world/ocean.h"
#include "world/weather.h"
#include "world/worldgen.h"

#include <math.h>
#include <stdio.h>

static void TestZonesAreOrdered(void)
{
    puts("ocean");

    WeatherInit(3u);

    Check("the surface is sunlit", OceanZoneAtDepth(0.0f) == OCEAN_SUNLIT, true);

    /* Walking down, the zone never goes back up. */
    OceanZone last = OCEAN_SUNLIT;
    int changes = 0;
    bool ordered = true;

    for (float d = 0.0f; d < 3000.0f; d += 5.0f)
    {
        OceanZone z = OceanZoneAtDepth(d);

        if (z < last) ordered = false;
        if (z != last) { changes++; last = z; }
    }

    Check("zones never run backwards", ordered, true);

    Check("and all four are reachable", changes == OCEAN_ZONE_COUNT - 1, true);
    Check("the bottom one is the abyss", last == OCEAN_ABYSS, true);
}

static void TestLightHalvesAndHalves(void)
{
    /* Beer-Lambert, not a linear fade: two of the same depth in a row
       must multiply, or the zones are decoration. */
    float one = OceanLight(300.0f);
    float two = OceanLight(600.0f);

    printf("    light: surface %.2f, 300 down %.2f, 600 down %.2f\n",
           (double)OceanLight(0.0f), (double)one, (double)two);

    Check("the surface is fully lit", OceanLight(0.0f) > 0.999f, true);
    Check("stacking depth multiplies it", fabsf(two - one * one) < 0.001f, true);
    Check("it never reaches zero", OceanLight(4000.0f) > 0.0f, true);

    float previous = 2.0f;
    bool falling = true;

    for (float d = 0.0f; d < 2000.0f; d += 10.0f)
    {
        float l = OceanLight(d);
        if (l >= previous) falling = false;
        previous = l;
    }

    Check("and it only ever falls", falling, true);

    /* The zone names have to mean something. */
    Check("the twilight zone is genuinely dim", OceanLight(500.0f) < 0.35f, true);
    Check("the midnight zone is genuinely dark", OceanLight(900.0f) < 0.12f, true);
}

static void TestChillFollowsTheThermocline(void)
{
    Check("the surface layer is not cold", OceanChill(50.0f) <= 0.0f, true);
    Check("past the thermocline it is fully cold", OceanChill(1000.0f) >= 1.0f, true);

    /* A band, not a step - a cliff here would make diving a coin flip. */
    float mid = OceanChill(410.0f);
    printf("    chill halfway through the thermocline: %.2f\n", (double)mid);

    Check("and the change between them is gradual", mid > 0.3f && mid < 0.7f, true);

    float previous = -1.0f;
    bool rising = true;

    for (float d = 0.0f; d < 2000.0f; d += 10.0f)
    {
        float c = OceanChill(d);
        if (c < previous) rising = false;
        if (c < 0.0f || c > 1.0f) rising = false;
        previous = c;
    }

    Check("chill only ever rises, and stays in range", rising, true);
}

static void TestCrushDepthIsSomewhereReal(void)
{
    Check("pressure starts at nothing", OceanPressure(0.0f) <= 0.0f, true);
    Check("and climbs with depth", OceanPressure(1000.0f) > OceanPressure(500.0f), true);

    /* A crush depth below the seabed would never do anything. */
    float deepest = -1.0f;

    for (int i = -400; i <= 400; i++)
    {
        float d = OceanFloorHeight(i) - WeatherWaterY();
        if (d > deepest) deepest = d;
    }

    printf("    deepest seabed %.0f, crush depth %.0f\n",
           (double)deepest, (double)OCEAN_CRUSH_DEPTH);

    Check("the crush depth is somewhere the cat can actually get to",
          OCEAN_CRUSH_DEPTH < deepest, true);
}

static void TestTheFloorIsContinuous(void)
{
    /* Neighbouring chunks derive the boundary from the shared index, so
       the interpolation has to land exactly on it - a seam in the seabed
       is a hole things swim through. */
    bool exact = true;

    for (int i = -50; i <= 50; i++)
    {
        float shared = OceanFloorHeight(i);
        float lerped = OceanFloorAt((float)i * CHUNK_WIDTH);

        if (fabsf(shared - lerped) > 0.5f) exact = false;
    }

    Check("the floor meets the boundary it was built from", exact, true);

    /* And nothing in between jumps. */
    float previous = OceanFloorAt(-40.0f * CHUNK_WIDTH);
    float biggest = 0.0f;

    for (float x = -40.0f * CHUNK_WIDTH; x < 40.0f * CHUNK_WIDTH; x += 32.0f)
    {
        float y = OceanFloorAt(x);
        float step = fabsf(y - previous);

        if (step > biggest) biggest = step;
        previous = y;
    }

    printf("    biggest floor step over 32 units: %.1f\n", (double)biggest);
    Check("and it never steps off a cliff", biggest < 40.0f, true);
}

static void TestThereIsAnOceanToSwimIn(void)
{
    /* The bathymetry has to put real water under the streets, or every
       depth-keyed system below the surface is keyed to nothing. */
    WeatherInit(3u);

    float water = WeatherWaterY();
    float shallowest = 1e9f, deepest = -1e9f, sum = 0.0f;
    int n = 0, spanning = 0;

    for (int i = -400; i <= 400; i++)
    {
        float d = OceanFloorHeight(i) - water;

        if (d < shallowest) shallowest = d;
        if (d > deepest) deepest = d;
        if (OceanZoneAtDepth(d) >= OCEAN_MIDNIGHT) spanning++;

        sum += d;
        n++;
    }

    printf("    seabed %.0f..%.0f down, mean %.0f, %d%% below the twilight zone\n",
           (double)shallowest, (double)deepest, (double)(sum / (float)n),
           spanning * 100 / n);

    Check("there is water everywhere, not just in places",
          shallowest > 300.0f, true);
    Check("and most of the seabed is past the reach of daylight",
          spanning * 2 > n, true);
}

void SuiteOcean(void)
{
    TestZonesAreOrdered();
    TestLightHalvesAndHalves();
    TestChillFollowsTheThermocline();
    TestCrushDepthIsSomewhereReal();
    TestTheFloorIsContinuous();
    TestThereIsAnOceanToSwimIn();
}
