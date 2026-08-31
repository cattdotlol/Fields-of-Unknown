#include "world/season.h"

#include <math.h>

/* Five minutes a season, so a year is twenty. Long enough to settle into,
   short enough that a session sees the world change. */
#define SEASON_LENGTH 300.0f

typedef struct Profile {
    const char *name;
    float temperature;   /* 0 freezing .. 1 warm */
    float rainBias;
    float bareness;
    Color leaf;
    Color leafHigh;
} Profile;

static const Profile PROFILES[SEASON_COUNT] = {
    { "SPRING", 0.58f, 1.15f, 0.10f, {  34,  74,  40, 255 }, {  56, 104,  58, 255 } },
    { "SUMMER", 0.92f, 0.70f, 0.05f, {  26,  62,  34, 255 }, {  40,  86,  46, 255 } },
    { "AUTUMN", 0.44f, 1.25f, 0.45f, {  86,  56,  26, 255 }, { 126,  80,  34, 255 } },
    { "WINTER", 0.10f, 0.85f, 0.85f, {  46,  52,  58, 255 }, {  68,  76,  84, 255 } },
};

static float sElapsed;

void SeasonInit(void)
{
    sElapsed = 0.0f;
}

void SeasonUpdate(float dt)
{
    sElapsed += dt;

    float year = SEASON_LENGTH * (float)SEASON_COUNT;
    if (sElapsed >= year) sElapsed -= year;
}

void SeasonSet(Season season)
{
    if (season < 0 || season >= SEASON_COUNT) return;

    sElapsed = (float)season * SEASON_LENGTH + SEASON_LENGTH * 0.25f;
}

Season SeasonCurrent(void)
{
    int index = (int)(sElapsed / SEASON_LENGTH);
    if (index < 0) index = 0;
    if (index >= SEASON_COUNT) index = SEASON_COUNT - 1;

    return (Season)index;
}

float SeasonProgress(void)
{
    return fmodf(sElapsed, SEASON_LENGTH) / SEASON_LENGTH;
}

const char *SeasonName(void)
{
    return PROFILES[SeasonCurrent()].name;
}

/* Everything seasonal eases into the next season over the back half, so
   the world never changes between one frame and the next. */
static float Blend(void)
{
    float p = SeasonProgress();
    return (p < 0.5f) ? 0.0f : (p - 0.5f) * 2.0f;
}

static Season NextSeason(void)
{
    return (Season)(((int)SeasonCurrent() + 1) % SEASON_COUNT);
}

float SeasonTemperature(void)
{
    const Profile *a = &PROFILES[SeasonCurrent()];
    const Profile *b = &PROFILES[NextSeason()];

    return a->temperature + (b->temperature - a->temperature) * Blend();
}

float SeasonRainBias(void)
{
    const Profile *a = &PROFILES[SeasonCurrent()];
    const Profile *b = &PROFILES[NextSeason()];

    return a->rainBias + (b->rainBias - a->rainBias) * Blend();
}

float SeasonBareness(void)
{
    const Profile *a = &PROFILES[SeasonCurrent()];
    const Profile *b = &PROFILES[NextSeason()];

    return a->bareness + (b->bareness - a->bareness) * Blend();
}

void SeasonFoliage(Color *leaf, Color *leafHigh)
{
    const Profile *a = &PROFILES[SeasonCurrent()];
    const Profile *b = &PROFILES[NextSeason()];
    float t = Blend();

    if (leaf)     *leaf = ColorLerp(a->leaf, b->leaf, t);
    if (leafHigh) *leafHigh = ColorLerp(a->leafHigh, b->leafHigh, t);
}
