#ifndef ENTITY_AQUATIC_H
#define ENTITY_AQUATIC_H

#include "entity/species.h"

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
   cat can follow.

   Fish are why any of it is worth the trip. They school, they scatter
   from anything that eats them, and a spooked one swims faster than the
   cat does - so they are caught the way rats are caught, by arriving
   before they know about it. They are also what the shark actually
   lives on, which is what makes the water a food chain the cat has
   walked into rather than a trap built around it. */

/* Fish are most of this: a school has to look like a school. */
#define AQUATIC_MAX 40

typedef enum AquaticKind {
    AQUA_JELLY = 0,
    AQUA_SHARK,
    AQUA_WHALE,
    AQUA_FISH,
    AQUA_KIND_COUNT
} AquaticKind;

void AquaticReset(void);
void AquaticFixedUpdate(float dt);
void AquaticDraw(float alpha, float left, float right);

int         AquaticCount(void);
int         AquaticCountOf(AquaticKind kind);
bool        AquaticActive(int index);
AquaticKind AquaticKindOf(int index);

/* The same animal as the rest of the game knows it. Kinds are this
   module's private vocabulary; the food web is written in species. */
Species     AquaticSpeciesOf(int index);
Vector2     AquaticPosition(int index);
float       AquaticGlow(int index);      /* 0 for anything that does not */
bool        AquaticHunting(int index);

/* How alarmed a fish is, 0 to 1 - the same number its catch reach is
   read off. Nothing in the game uses it; the tests and the debug overlay
   do, because a school that never calms down looks identical from the
   outside to one that was never frightened. */
float       AquaticAlarm(int index);

void AquaticForceSpawn(AquaticKind kind, float x);   /* dev tools */

#endif /* ENTITY_AQUATIC_H */
