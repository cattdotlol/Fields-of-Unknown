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

/* --- steering and probes ----------------------------------------------
   The three things every walking creature needs from the world, kept here
   rather than copied into each of them. */

/* Ease horizontal velocity toward a wanted speed rather than snapping to
   it. Everything that walks accelerates; assigning velocity directly is
   what makes a creature read as a sliding block instead of an animal. */
void BodySteerX(Body *b, float wanted, float accel, float dt);

/* Is there floor just ahead at the height it is standing on? The probe is
   `reach` across, that far in front of the body, and looks `depth` down.
   Nothing sensible walks off a ledge into open water. */
bool BodyGroundAhead(const Body *b, float facing, float reach, float depth);

/* Something solid at chest height in front: a wall it might hop rather
   than a drop it should stop at. Covers the lower four fifths of the
   body, so a gap it can duck under does not read as a wall. */
bool BodyWallAhead(const Body *b, float facing, float reach, float width);

#endif /* WORLD_PHYSICS_H */
