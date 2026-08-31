#ifndef ENTITY_CAT_H
#define ENTITY_CAT_H

#include "raylib.h"

#include <stdbool.h>

typedef enum CatState {
    CAT_IDLE = 0,
    CAT_WALK,
    CAT_RUN,
    CAT_CROUCH,
    CAT_AIR,
    CAT_SWIM
} CatState;

void CatSpawn(Vector2 position);

/* Simulation. Fixed timestep only. */
void CatFixedUpdate(float dt);

/* alpha is AppRenderAlpha(): where the cat is between the last two ticks. */
void CatDraw(float alpha);

Vector2   CatPosition(void);              /* bottom-centre, world units */
Vector2   CatRenderPosition(float alpha); /* interpolated, for the camera */
Rectangle CatBounds(void);
CatState  CatCurrentState(void);
bool      CatIsSwimming(void);
bool      CatIsSubmerged(void);   /* head under, not just floating */

/* 0..1 after weather masking - how far the cat's noise carries. Stealth
   and predators will read this. */
float CatNoise(void);
float CatVelocityX(void);

/* Derived from the movement constants. World generation reads these so a
   change to gravity or jump strength cannot silently produce a level the
   cat is unable to cross. */
/* Knockback, for anything that hits the cat. */
void CatShove(float vx, float vy);

float CatMaxJumpHeight(void);

/* Tuning, exposed so it can be asserted instead of drifting. */
float CatStrideRate(void);
float CatRunSpeed(void);
float CatSwimRestDepth(void);    /* where buoyancy parks the cat  */
float CatSwimKickWindow(void);   /* how deep it can still push off */
float CatMaxRunJumpDistance(void);

#endif /* ENTITY_CAT_H */
