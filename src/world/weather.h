#ifndef WORLD_WEATHER_H
#define WORLD_WEATHER_H

#include <stdbool.h>

/* Weather is the difficulty dial, not set dressing. Rain hides the cat's
   scent and noise but raises the water and closes routes; dry weather
   opens the map back up and leaves the cat trackable. */

typedef enum WeatherState {
    WEATHER_DRY = 0,
    WEATHER_DRIZZLE,
    WEATHER_RAIN,
    WEATHER_STORM,
    WEATHER_STATE_COUNT
} WeatherState;

void WeatherInit(unsigned int seed);
void WeatherUpdate(float dt);

float WeatherRain(void);        /* 0..1, smoothed */
float WeatherWind(void);        /* -1..1 */
float WeatherWetness(void);     /* 0..1, integrated rain; drives the water */
float WeatherWaterY(void);      /* world y of the surface (smaller = higher) */

/* Where the water tops out when everything is as wet as it gets. World
   generation needs this to guarantee a dry route through every chunk. */
float WeatherMaxWaterY(void);

/* 1 = fully covered, 0 = fully exposed. Stealth reads these. */
float WeatherScentMask(void);
float WeatherNoiseMask(void);

/* Below freezing with something falling: the same precipitation, drawn
   and felt differently. */
bool WeatherIsSnow(void);

/* 0..1, spikes on a lightning strike and decays. Drawn as a flash. */
float WeatherFlash(void);

/* True once per strike, when the rumble should reach the cat - which is
   after the flash, by however far away it was. */
bool WeatherConsumeThunder(float *loudness);

WeatherState WeatherCurrent(void);
const char  *WeatherName(void);

#endif /* WORLD_WEATHER_H */
