#ifndef WORLD_SEASON_H
#define WORLD_SEASON_H

#include "raylib.h"

/* The year turns underneath the weather. Seasons bias what the sky does,
   how fast the cat loses heat, and what the trees look like - so the same
   stretch of sprawl is a different problem in winter than in summer. */

typedef enum Season {
    SEASON_SPRING = 0,
    SEASON_SUMMER,
    SEASON_AUTUMN,
    SEASON_WINTER,
    SEASON_COUNT
} Season;

void SeasonInit(void);
void SeasonUpdate(float dt);

Season      SeasonCurrent(void);

/* Dev tools only. */
void SeasonSet(Season season);
const char *SeasonName(void);
float       SeasonProgress(void);     /* 0..1 through the current season */

/* 0 = freezing, 1 = warm. Eased across the boundary so nothing snaps. */
float SeasonTemperature(void);

/* Multiplies the weather's rain target. */
float SeasonRainBias(void);

/* Canopy colours for the season, blended toward the next one. */
void SeasonFoliage(Color *leaf, Color *leafHigh);

/* Fraction of trees that have dropped their leaves. */
float SeasonBareness(void);

#endif /* WORLD_SEASON_H */
