#ifndef WORLD_MUSHROOM_H
#define WORLD_MUSHROOM_H

#include "raylib.h"

#include <stdbool.h>

/* Food, and the first thing in the world that can hurt you.

   Species are not labelled anywhere. Some feed you, some poison you, and
   the only way to find out which is to eat one and see what happens. */

#define MUSHROOM_SPECIES 6

typedef struct Mushroom {
    float x;
    float baseY;              /* ground it grows on */
    unsigned int variant;
    unsigned char species;
} Mushroom;

typedef struct MushroomEffect {
    float hunger;             /* negative for the bad ones */
    float health;
    float warmth;
} MushroomEffect;

MushroomEffect MushroomEffectOf(unsigned char species);
Color          MushroomCapColor(unsigned char species);

void MushroomDraw(const Mushroom *m, bool highlight);

/* Eaten mushrooms stay eaten for a while, then grow back. The table is
   fixed size and forgets the oldest entries, which is both bounded and
   roughly how mushrooms behave. */
void MushroomClearHarvests(void);
void MushroomTick(float dt);
bool MushroomIsHarvested(int chunkIndex, int slot);
void MushroomHarvest(int chunkIndex, int slot);

#endif /* WORLD_MUSHROOM_H */
