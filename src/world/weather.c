#include "world/weather.h"

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
static float sTime;

static float Rand01(void)
{
    return (float)GetRandomValue(0, 10000) / 10000.0f;
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
    SetRandomSeed(seed);

    sTime = 0.0f;
    sWetness = 0.10f;   /* the ground starts dry; flooding is earned */
    sWind = 0.0f;

    EnterState(WEATHER_DRIZZLE);   /* it is wet, but not yet rising */
    sRain = sTarget;
}

void WeatherUpdate(float dt)
{
    sTime += dt;

    sHold -= dt;
    if (sHold <= 0.0f) EnterState(NextState(sState));

    /* Ease toward the target instead of snapping. */
    sRain += (sTarget - sRain) * RAIN_LERP * dt;
    sWind += (sWindTarget - sWind) * 0.20f * dt;

    /* Gusts, so wind never sits perfectly still. */
    float gust = sinf(sTime * 0.7f) * sinf(sTime * 0.23f) * 0.15f * sRain;

    if (sRain > WET_THRESHOLD) sWetness += (sRain - WET_THRESHOLD) * WET_FILL_RATE * dt;
    else                       sWetness -= WET_DRAIN_RATE * dt;

    if (sWetness < 0.0f) sWetness = 0.0f;
    if (sWetness > 1.0f) sWetness = 1.0f;

    sWind += gust * dt;
    if (sWind < -1.0f) sWind = -1.0f;
    if (sWind >  1.0f) sWind =  1.0f;
}

float WeatherRain(void)     { return sRain; }
float WeatherWind(void)     { return sWind; }
float WeatherWetness(void)  { return sWetness; }

float WeatherWaterY(void)
{
    return WATER_BASE_Y - sWetness * WATER_RISE;
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

WeatherState WeatherCurrent(void) { return sState; }

const char *WeatherName(void) { return NAMES[sState]; }
