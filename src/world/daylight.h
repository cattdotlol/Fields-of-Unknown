#ifndef WORLD_DAYLIGHT_H
#define WORLD_DAYLIGHT_H

#include "raylib.h"

/* The planet turns. Day is for covering ground; night is dark, cold, and
   belongs to whatever else lives here. Nothing announces the hour - the
   cat reads the sky like everything else does. */

typedef enum DayPhase {
    PHASE_NIGHT = 0,
    PHASE_DAWN,
    PHASE_DAY,
    PHASE_DUSK,
    PHASE_COUNT
} DayPhase;

/* One full turn of the planet, in seconds. */
#define DAY_LENGTH 960.0f

void DaylightInit(void);
void DaylightUpdate(float dt);

/* 0 is midnight, 0.5 is noon. */
float    DaylightTime(void);
int      DaylightDay(void);          /* days survived, counting from 1 */
DayPhase DaylightPhase(void);

/* 0 in the dead of night, 1 at noon. Already includes the moon's share,
   so it never quite reaches zero on a clear night. */
float DaylightBrightness(void);

/* How much harder the cold bites after dark. 0 by day, 1 at midnight. */
float DaylightChill(void);

/* Sky gradient for the hour. */
void DaylightSky(Color *top, Color *bottom);

/* How much of the starfield shows through. */
float DaylightStarAlpha(void);

/* -1 is below the horizon, 0 is on it, 1 is directly overhead. */
float DaylightSunHeight(void);
float DaylightMoonHeight(void);

/* 0 is new, 1 is full. Runs on its own cycle, so some nights are darker
   than others and the cat has no say in which. */
float DaylightMoonFullness(void);

/* Where the body sits along its arc, 0 rising to 1 setting. */
float DaylightSunTrack(void);
float DaylightMoonTrack(void);

/* Dev tools only. */
const char *DaylightPhaseName(void);
void        DaylightSetTime(float t);
void        DaylightSkip(int phases);

#endif /* WORLD_DAYLIGHT_H */
