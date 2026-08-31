#include "world/daylight.h"

#include <math.h>

/* Where the sun crosses the horizon. Everything else is derived. */
#define SUNRISE  0.25f
#define SUNSET   0.75f

/* Nights are not all alike: the moon runs an eight day cycle underneath
   the daily one. */
#define MOON_CYCLE 8

static float sTime = 0.30f;      /* start after first light, not before it */
static int   sDay  = 1;

static float Clamp01(float v)
{
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static float Lerp(float a, float b, float t)
{
    return a + (b - a) * t;
}

void DaylightInit(void)
{
    sTime = 0.30f;
    sDay  = 1;
}

void DaylightUpdate(float dt)
{
    sTime += dt / DAY_LENGTH;

    while (sTime >= 1.0f)
    {
        sTime -= 1.0f;
        sDay++;
    }
}

float DaylightTime(void) { return sTime; }
int   DaylightDay(void)  { return sDay; }

/* Elevation of a body that rises at `rise` and sets half a turn later. */
static float Elevation(float t, float rise)
{
    return sinf((t - rise) * 2.0f * PI);
}

float DaylightSunHeight(void)
{
    return Elevation(sTime, SUNRISE);
}

float DaylightMoonHeight(void)
{
    return Elevation(sTime, SUNSET);
}

float DaylightMoonFullness(void)
{
    float phase = (float)(sDay % MOON_CYCLE) / (float)MOON_CYCLE;
    return 0.5f - 0.5f * cosf(phase * 2.0f * PI);
}

/* How much of the sun is actually doing anything. Zero a little before it
   touches the horizon, so dusk has somewhere to go. */
static float SunShare(void)
{
    return Clamp01((DaylightSunHeight() + 0.12f) / 0.55f);
}

float DaylightBrightness(void)
{
    float sun = SunShare();

    float moon = DaylightMoonHeight();
    if (moon < 0.0f) moon = 0.0f;

    /* The moon only matters once the sun has gone - but then it matters a
       lot. A full moon is a night you can cross ground on; a new one is
       not. */
    float b = sun + (1.0f - sun) * moon * DaylightMoonFullness() * 0.30f;

    /* Starlight. Never truly pitch black, or the screen is just off. */
    return (b < 0.06f) ? 0.06f : b;
}

float DaylightChill(void)
{
    return Clamp01((0.35f - SunShare()) / 0.35f);
}

DayPhase DaylightPhase(void)
{
    if (sTime < 0.20f) return PHASE_NIGHT;
    if (sTime < 0.32f) return PHASE_DAWN;
    if (sTime < 0.70f) return PHASE_DAY;
    if (sTime < 0.82f) return PHASE_DUSK;
    return PHASE_NIGHT;
}

const char *DaylightPhaseName(void)
{
    switch (DaylightPhase())
    {
        case PHASE_DAWN:  return "dawn";
        case PHASE_DAY:   return "day";
        case PHASE_DUSK:  return "dusk";
        default:          return "night";
    }
}

float DaylightStarAlpha(void)
{
    /* Stars wash out as soon as there is any real light in the sky. */
    return Clamp01(1.0f - SunShare() * 3.4f);
}

float DaylightSunTrack(void)
{
    return Clamp01((sTime - SUNRISE) / (SUNSET - SUNRISE));
}

float DaylightMoonTrack(void)
{
    float t = (sTime >= SUNSET) ? (sTime - SUNSET) : (sTime + (1.0f - SUNSET));
    return Clamp01(t / 0.5f);
}

/* --- sky ---------------------------------------------------------------
   Keyframes around the turn. The palette stays off-key on purpose: this
   is not Earth's sky, so noon washes teal rather than blue. */

typedef struct SkyKey {
    float at;
    Color top;
    Color bottom;
} SkyKey;

static const SkyKey SKY[] = {
    { 0.00f, {  12,  10,  24, 255 }, {  20,  30,  34, 255 } },  /* midnight   */
    { 0.18f, {  16,  14,  32, 255 }, {  26,  38,  42, 255 } },  /* late night */
    { 0.24f, {  38,  30,  58, 255 }, {  86,  60,  64, 255 } },  /* first light*/
    { 0.29f, {  74,  62, 104, 255 }, { 206, 116,  80, 255 } },  /* dawn       */
    { 0.36f, {  78, 112, 138, 255 }, { 168, 184, 168, 255 } },  /* morning    */
    { 0.50f, {  84, 134, 156, 255 }, { 184, 202, 186, 255 } },  /* noon       */
    { 0.68f, {  78, 118, 146, 255 }, { 174, 188, 172, 255 } },  /* afternoon  */
    { 0.78f, {  70,  64, 102, 255 }, { 212, 120,  74, 255 } },  /* low sun    */
    { 0.85f, {  34,  26,  54, 255 }, {  92,  58,  60, 255 } },  /* dusk       */
    { 0.92f, {  16,  14,  32, 255 }, {  26,  38,  42, 255 } },  /* nightfall  */
};

#define SKY_KEYS ((int)(sizeof(SKY) / sizeof(SKY[0])))

static Color Blend(Color a, Color b, float t)
{
    Color c;
    c.r = (unsigned char)Lerp((float)a.r, (float)b.r, t);
    c.g = (unsigned char)Lerp((float)a.g, (float)b.g, t);
    c.b = (unsigned char)Lerp((float)a.b, (float)b.b, t);
    c.a = 255;
    return c;
}

void DaylightSky(Color *top, Color *bottom)
{
    int i = SKY_KEYS - 1;
    while (i > 0 && SKY[i].at > sTime) i--;

    int j = (i + 1) % SKY_KEYS;

    /* The last key wraps back round to the first, a turn later. */
    float from = SKY[i].at;
    float to   = (j == 0) ? 1.0f : SKY[j].at;

    float span = to - from;
    float t = (span > 0.0f) ? (sTime - from) / span : 0.0f;

    if (top)    *top    = Blend(SKY[i].top,    SKY[j].top,    t);
    if (bottom) *bottom = Blend(SKY[i].bottom, SKY[j].bottom, t);
}

void DaylightSetTime(float t)
{
    t = t - floorf(t);
    sTime = t;
}

void DaylightSkip(int phases)
{
    /* Jump to the start of the next phase, however many times asked. */
    static const float STARTS[] = { 0.82f, 0.20f, 0.32f, 0.70f };

    for (int i = 0; i < phases; i++)
    {
        DayPhase p = DaylightPhase();
        int next = ((int)p + 1) % PHASE_COUNT;

        float target = STARTS[next];
        if (target <= sTime) sDay++;

        sTime = target + 0.001f;
        if (sTime >= 1.0f) sTime -= 1.0f;
    }
}
