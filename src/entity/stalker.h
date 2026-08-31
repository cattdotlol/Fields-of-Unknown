#ifndef ENTITY_STALKER_H
#define ENTITY_STALKER_H

#include "raylib.h"

#include <stdbool.h>

/* The thing from the intro. Where a rat runs from noise, this walks
   toward it - so the cat's own movement is what calls it in.

   Counterplay is built into the numbers rather than bolted on: it is
   slower than a sprinting cat but sprinting is the loudest thing you can
   do, it will not follow across water, and it loses interest if it
   cannot find you. Going quiet is always an answer. */

#define STALKER_MAX 2

typedef enum StalkerState {
    STALK_PROWL = 0,   /* has not heard anything                    */
    STALK_HUNT,        /* moving to where the noise came from       */
    STALK_SEARCH,      /* got there, casting about                  */
    STALK_STRIKE,      /* committed to a lunge                      */
    STALK_WITHDRAW     /* backing off after connecting              */
} StalkerState;

void StalkersReset(void);
void StalkersForceSpawn(float x);   /* dev tools */
void StalkersFixedUpdate(float dt);
void StalkersDraw(float alpha, float left, float right);

int          StalkerCount(void);
int          StalkerHunting(void);      /* how many are actively after you */
bool         StalkerActive(int index);
Vector2      StalkerPosition(int index);
StalkerState StalkerCurrentState(int index);
float        StalkerInterest(int index);

/* True once, when one of them commits to hunting. The audio layer turns
   this into the sound you learn to dread. */
bool StalkerConsumeRoar(float *loudness);

/* Loudest thing on screen, for the audio layer to react to. */
float StalkerNearestDistance(void);

#endif /* ENTITY_STALKER_H */
