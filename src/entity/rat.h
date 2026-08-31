#ifndef ENTITY_RAT_H
#define ENTITY_RAT_H

#include "raylib.h"

#include <stdbool.h>

/* Rats are the first thing in the world with a mind of its own, and the
   first real meal. They hear the cat rather than see it, so how you move
   decides whether you eat.

   A fixed pool that lives around the player: the world is endless, so
   population is maintained near the cat rather than stored per chunk. */

#define RAT_MAX 20

typedef enum RatState {
    RAT_WANDER = 0,
    RAT_PAUSE,
    RAT_FLEE
} RatState;

void RatsReset(void);
void RatsFixedUpdate(float dt);
void RatsDraw(float alpha, float left, float right);

/* Index of a rat close enough to grab, or -1. Fleeing rats need the cat
   to be much closer. */
int  RatCatchable(Rectangle catBox);
void RatConsume(int index);

int  RatCount(void);
int  RatAlarmed(void);      /* how many are currently running away */

/* Inspection, for the debug overlay and the tests. */
bool     RatActive(int index);
Vector2  RatPosition(int index);
RatState RatCurrentState(int index);
float    RatAlertLevel(int index);

#endif /* ENTITY_RAT_H */
