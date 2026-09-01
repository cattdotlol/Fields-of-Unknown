#ifndef ENTITY_AQUATIC_H
#define ENTITY_AQUATIC_H

#include "raylib.h"

#include <stdbool.h>

/* What lives in the flood.

   Land has the stalker; water gets its own reasons to be careful. A shark
   swims faster than the cat can, so once one has noticed you the answer
   is to get out, not to outswim it - which is what makes breath and
   diving matter. Jellyfish are harmless: they drift, they glow, and
   below the surface they are the only light there is.
   Whales are neither: they are just enormous and pass by. */

#define AQUATIC_MAX 14

typedef enum AquaticKind {
    AQUA_JELLY = 0,
    AQUA_SHARK,
    AQUA_WHALE,
    AQUA_KIND_COUNT
} AquaticKind;

void AquaticReset(void);
void AquaticFixedUpdate(float dt);
void AquaticDraw(float alpha, float left, float right);

int         AquaticCount(void);
int         AquaticCountOf(AquaticKind kind);
bool        AquaticActive(int index);
AquaticKind AquaticKindOf(int index);
Vector2     AquaticPosition(int index);
float       AquaticGlow(int index);      /* 0 for anything that does not */
bool        AquaticHunting(int index);

void AquaticForceSpawn(AquaticKind kind, float x);   /* dev tools */

#endif /* ENTITY_AQUATIC_H */
