#ifndef ENTITY_AQUATIC_H
#define ENTITY_AQUATIC_H

#include "raylib.h"

#include <stdbool.h>

/* What lives in the flood.

   The drowned city is a lid on the sea. Nothing out here lives above
   about three hundred down, because above that the water is more solid
   than not - only the channels get through, and only a cat fits them.
   So none of this is ever seen from the surface: it is all found by
   diving, which is what makes breath matter.

   Below that lid the whole layer moves with the sun. It rises after
   dark and sinks again at first light, the way the real one does, so
   the same dive is a different sea at midnight than it is at noon.

   Land has the stalker; water gets its own reasons to be careful. A
   shark swims faster than the cat can, so once one has noticed you the
   answer is to get out, not to outswim it. Jellyfish are harmless: they
   drift, they glow, and down there they are the only light. The whale
   is neither - it works the whole column, and it goes deeper than the
   cat can follow. */

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
