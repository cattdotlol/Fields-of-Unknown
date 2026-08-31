#include "world/weather.h"
#include "world/season.h"

#include "raylib.h"

#include <math.h>

/* World-space waterline. +y is down, so a rising flood means a smaller y. */
#define WATER_BASE_Y   578.0f
#define WATER_RISE      72.0f

/* How fast standing water answers the sky. Draining is deliberately
   slower than filling - the sprawl holds what it takes. */
#define WET_FILL_RATE   0.011f
#define WET_DRAIN_RATE  0.007f
#define WET_THRESHOLD   0.30f   /* rain above this adds water */

#define RAIN_LERP       0.35f   /* seconds^-1 toward the target */

typedef struct Profile {
    float rain;
    float minHold, maxHold;
} Profile;

/* Target rain per state, and how long it tends to sit there. */
static const Profile PROFILES[WEATHER_STATE_COUNT] = {
    { 0.00f, 40.0f, 110.0f },   /* DRY     */
    { 0.28f, 30.0f,  80.0f },   /* DRIZZLE */
    { 0.68f, 35.0f,  95.0f },   /* RAIN    */
    { 1.00f, 20.0f,  55.0f },   /* STORM   */
};

static const char *NAMES[WEATHER_STATE_COUNT] = { "DRY", "DRIZZLE", "RAIN", "STORM" };

static WeatherState sState;
static float sRain;        /* smoothed, what everything reads */
static float sTarget;
static float sHold;        /* seconds left in this state */
static float sWetness;
static float sWind;
static float sWindTarget;
static float sGust;
static float sTime;

/* Lightning. A strike flashes immediately and the sound arrives later,
   the way it does. */
static float sFlash;
static float sStrikeIn;
static float sThunderIn;
static float sThunderLoud;
static bool  sThunderPending;

/* Its own generator, not raylib's global one. Film grain draws hundreds
   of numbers a frame from that, so sharing it made the weather depend on
   whether the intro happened to be on screen - and stop being
   reproducible from WORLD_SEED at all. */
static unsigned int sRng = 1u;

static float Rand01(void)
{
    sRng = sRng * 1664525u + 1013904223u;
    return (float)((sRng >> 8) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

/* Weather drifts a step at a time rather than teleporting from dry to
   storm, so the player can read it coming. */
static WeatherState NextState(WeatherState from)
{
    int step = (Rand01() > 0.5f) ? 1 : -1;

    /* At the ends, the only way is inward. */
    if (from == WEATHER_DRY) step = 1;
    if (from == WEATHER_STORM) step = -1;

    /* Occasionally hold and just re-roll the duration. */
    if (Rand01() > 0.82f) return from;

    int next = (int)from + step;
    if (next < 0) next = 0;
    if (next >= WEATHER_STATE_COUNT) next = WEATHER_STATE_COUNT - 1;

    return (WeatherState)next;
}

static void EnterState(WeatherState s)
{
    sState = s;
    sTarget = PROFILES[s].rain;
    sHold = PROFILES[s].minHold + Rand01() * (PROFILES[s].maxHold - PROFILES[s].minHold);
    sWindTarget = (Rand01() * 2.0f - 1.0f) * (0.35f + sTarget * 0.65f);
}

void WeatherInit(unsigned int seed)
{
    sRng = seed | 1u;

    sTime = 0.0f;
    sWetness = 0.10f;   /* the ground starts dry; flooding is earned */
    sWind = 0.0f;
    sGust = 0.0f;
    sFlash = 0.0f;
    sStrikeIn = 20.0f;
    sThunderIn = 0.0f;
    sThunderLoud = 0.0f;
    sThunderPending = false;

    EnterState(WEATHER_DRIZZLE);   /* it is wet, but not yet rising */
    sRain = sTarget;
}

void WeatherUpdate(float dt)
{
    sTime += dt;

    sHold -= dt;
    if (sHold <= 0.0f) EnterState(NextState(sState));

    /* The season decides how wet this state actually is. */
    float target = sTarget * SeasonRainBias();
    if (target > 1.0f) target = 1.0f;

    sRain += (target - sRain) * RAIN_LERP * dt;
    sWind += (sWindTarget - sWind) * 0.20f * dt;

    /* Gusts. Three rates that do not divide into each other, so it never
       settles into an audible loop, and present in dry weather too - the
       old version multiplied by rain, so a clear day was dead still. */
    sGust = sinf(sTime * 0.53f) * 0.50f
          + sinf(sTime * 1.31f) * 0.30f
          + sinf(sTime * 2.70f) * 0.20f;

    sGust *= 0.22f + sRain * 0.45f;

    if (sRain > WET_THRESHOLD) sWetness += (sRain - WET_THRESHOLD) * WET_FILL_RATE * dt;
    else                       sWetness -= WET_DRAIN_RATE * dt;

    if (sWetness < 0.0f) sWetness = 0.0f;
    if (sWetness > 1.0f) sWetness = 1.0f;

    /* --- lightning --------------------------------------------------- */
    if (sFlash > 0.0f) sFlash -= dt * 3.2f;

    if (sRain > 0.55f)
    {
        sStrikeIn -= dt * (0.5f + sRain);

        if (sStrikeIn <= 0.0f)
        {
            /* Distance decides both the delay and how loud it lands. */
            float distance = Rand01();

            sFlash = 1.0f - distance * 0.45f;
            sThunderIn = 0.35f + distance * 4.5f;
            sThunderLoud = 1.0f - distance * 0.65f;

            sStrikeIn = 7.0f + Rand01() * 26.0f * (1.2f - sRain);
        }
    }

    if (sThunderIn > 0.0f)
    {
        sThunderIn -= dt;
        if (sThunderIn <= 0.0f) sThunderPending = true;
    }

    if (sWind < -1.0f) sWind = -1.0f;
    if (sWind >  1.0f) sWind =  1.0f;
}

float WeatherRain(void)     { return sRain; }
float WeatherWind(void)
{
    float w = sWind + sGust;

    if (w < -1.0f) w = -1.0f;
    if (w >  1.0f) w =  1.0f;

    return w;
}
float WeatherWetness(void)  { return sWetness; }

float WeatherWaterY(void)
{
    return WATER_BASE_Y - sWetness * WATER_RISE;
}

float WeatherMaxWaterY(void)
{
    return WATER_BASE_Y - WATER_RISE;
}

float WeatherBaseWaterY(void)
{
    return WATER_BASE_Y;
}

/* Rain beats scent down fast; it takes a while to come back. */
float WeatherScentMask(void)
{
    float m = sRain * 1.15f;
    return (m > 1.0f) ? 1.0f : m;
}

/* A downpour covers footfalls, but only so much. */
float WeatherNoiseMask(void)
{
    return sRain * 0.75f;
}

float WeatherFlash(void)
{
    return (sFlash > 0.0f) ? sFlash : 0.0f;
}

bool WeatherConsumeThunder(float *loudness)
{
    if (!sThunderPending) return false;

    sThunderPending = false;
    if (loudness) *loudness = sThunderLoud;

    return true;
}

bool WeatherIsSnow(void)
{
    return SeasonTemperature() < 0.32f && sRain > 0.05f;
}

void WeatherForceState(WeatherState state)
{
    if (state < 0 || state >= WEATHER_STATE_COUNT) return;

    EnterState(state);
    sRain = sTarget;        /* skip the ramp; this is a dev jump */
}

void WeatherSetWetness(float wetness)
{
    if (wetness < 0.0f) wetness = 0.0f;
    if (wetness > 1.0f) wetness = 1.0f;

    sWetness = wetness;
}

WeatherState WeatherCurrent(void) { return sState; }

const char *WeatherName(void) { return NAMES[sState]; }
