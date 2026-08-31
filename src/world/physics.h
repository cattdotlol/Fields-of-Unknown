#ifndef WORLD_PHYSICS_H
#define WORLD_PHYSICS_H

#include "raylib.h"

#include <stdbool.h>

/* An axis-aligned body that collides with the terrain. Every moving
   creature owns one of these; the resolution code lives here exactly
   once. Position is bottom-centre, +y down, world units = design pixels. */

typedef struct Body {
    Vector2 pos;
    Vector2 prevPos;     /* start of the current tick, for render lerp */
    Vector2 vel;
    float   width;
    float   height;
    bool    grounded;
} Body;

void BodyInit(Body *b, Vector2 pos, float width, float height);

Rectangle BodyRect(const Body *b);
Rectangle BodyRectAt(const Body *b, Vector2 pos, float height);

/* Call at the top of each fixed tick, before anything moves. */
void BodyBeginTick(Body *b);

void BodyApplyGravity(Body *b, float gravity, float maxFall, float dt);

/* Axis-separated so corners do not snag; clears velocity on contact and
   sets `grounded` when it lands. */
void BodyMove(Body *b, float dt);

/* Where to draw it this frame: alpha is the leftover fraction of a tick. */
Vector2 BodyRenderPos(const Body *b, float alpha);

#endif /* WORLD_PHYSICS_H */
