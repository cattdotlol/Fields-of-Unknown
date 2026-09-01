#include "world/ocean.h"
#include "world/weather.h"
#include "world/worldgen.h"

#include <math.h>

/* Tuned against the cat: sixteen seconds of breath at a swim speed of 74
   is about 1180 units of travel, so a round trip on one lungful reaches
   roughly 500 down. The shelf sits inside that. Everything below it has
   to be earned with air pockets. */
#define ZONE_SUNLIT     280.0f
#define ZONE_TWILIGHT   760.0f
#define ZONE_MIDNIGHT  1600.0f

/* Beer-Lambert: light does not fade linearly, it halves and halves. */
#define LIGHT_DEPTH     420.0f

/* Real thermoclines are a band, not a line. */
#define THERMO_START    140.0f
#define THERMO_END      680.0f

float OceanDepthAt(float worldY)
{
    float d = worldY - WeatherWaterY();
    return (d < 0.0f) ? 0.0f : d;
}

OceanZone OceanZoneAtDepth(float depth)
{
    if (depth < ZONE_SUNLIT)   return OCEAN_SUNLIT;
    if (depth < ZONE_TWILIGHT) return OCEAN_TWILIGHT;
    if (depth < ZONE_MIDNIGHT) return OCEAN_MIDNIGHT;

    return OCEAN_ABYSS;
}

const char *OceanZoneName(OceanZone zone)
{
    switch (zone)
    {
        case OCEAN_SUNLIT:   return "SUNLIT";
        case OCEAN_TWILIGHT: return "TWILIGHT";
        case OCEAN_MIDNIGHT: return "MIDNIGHT";
        default:             return "ABYSS";
    }
}

float OceanLight(float depth)
{
    if (depth <= 0.0f) return 1.0f;

    return expf(-depth / LIGHT_DEPTH);
}

float OceanChill(float depth)
{
    if (depth <= THERMO_START) return 0.0f;
    if (depth >= THERMO_END)   return 1.0f;

    return (depth - THERMO_START) / (THERMO_END - THERMO_START);
}

float OceanPressure(float depth)
{
    return depth / 1000.0f;
}

/* --- bathymetry --------------------------------------------------------
   Three octaves, the lowest frequency dominating, so the floor reads as a
   shelf that breaks into a slope and settles onto a plain rather than as
   noise. */

static unsigned int Hash(int a, unsigned int salt)
{
    unsigned int h = (unsigned int)a * 2654435761u ^ (salt + 0x9E3779B9u) * 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

static float Hash01(int a, unsigned int salt)
{
    return (float)(Hash(a, salt) & 0xFFFFu) / 65535.0f;
}

/* Floor division, so the low-frequency octaves do not mirror around zero. */
static int Div(int a, int b)
{
    int q = a / b;
    if (a < 0 && (a % b) != 0) q -= 1;
    return q;
}

/* Smooth between control points instead of stepping between them. */
static float Octave(int index, int period, unsigned int salt)
{
    int cell = Div(index, period);
    float t = (float)(index - cell * period) / (float)period;

    float a = Hash01(cell, salt);
    float b = Hash01(cell + 1, salt);

    float smooth = t * t * (3.0f - 2.0f * t);

    return a + (b - a) * smooth;
}

float OceanFloorHeight(int boundaryIndex)
{
    float fine   = Octave(boundaryIndex, 2, 11u);
    float medium = Octave(boundaryIndex, 7, 23u);
    float broad  = Octave(boundaryIndex, 23, 47u);

    /* Broad structure decides shelf versus plain; the rest is relief. */
    float t = broad * 0.62f + medium * 0.26f + fine * 0.12f;

    /* A shelf break: the middle of the range is steepened so the floor
       tends to sit shallow or deep rather than halfway, the way a real
       continental margin does. */
    float shaped = t * t * (3.0f - 2.0f * t);

    return 900.0f + shaped * 2000.0f;
}

float OceanFloorAt(float worldX)
{
    float where = worldX / CHUNK_WIDTH;
    int   index = (int)floorf(where);
    float t = where - (float)index;

    float a = OceanFloorHeight(index);
    float b = OceanFloorHeight(index + 1);

    /* No clamp needed: OceanFloorHeight bottoms out at SEA_CEILING
       already, so a blend of two of them cannot come up above it. */
    return a + (b - a) * t;
}
