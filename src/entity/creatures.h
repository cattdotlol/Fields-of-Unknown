#ifndef ENTITY_CREATURES_H
#define ENTITY_CREATURES_H

#include "entity/species.h"

#include "raylib.h"

#include <stdbool.h>

/* Everything alive right now, in one list.

   Each creature module still owns its own animals and its own behaviour.
   What it does not own any more is the question "what else is near me",
   because that question is the food chain and every animal asks it. Rats
   fleeing, stalkers hunting, sharks feeding and the cat eating are all
   the same two queries against this, filtered through the diet table in
   species.h - so a new animal becomes prey to the right things the moment
   it is added there, without editing a single predator.

   Reads see the census as of the end of the previous tick. That is
   deliberate: it makes the answer independent of which module happens to
   update first, which is the bug you otherwise get months later when a
   fifth animal is inserted into the update order. A sixtieth of a second
   of staleness is not a perception system's problem - it is roughly what
   perception is. */

#define CREATURES_MAX 96

typedef struct Creature {
    Species species;
    Vector2 pos;
    float   width;
    float   height;

    /* The owning module's own index, handed back when something acts on
       this creature. Meaningless to anybody else. */
    int     tag;

    /* How close something has to get to take hold of it. A rat that has
       noticed you is far harder to grab than one that has not, which is
       the whole rat minigame, so the reach is published per tick by the
       module that knows. Zero means it cannot be caught at all. */
    float   reach;
} Creature;

void CreaturesReset(void);

/* Swap the buffers: what was published during the last tick becomes what
   queries see, and publishing starts again from empty. Called once at the
   top of the world tick, before anything moves. */
void CreaturesBeginTick(void);

/* Called by each creature module, once per live animal per tick. */
void CreaturesPublish(Species s, Vector2 pos, int tag, float reach);

int             CreaturesCount(void);
int             CreaturesCountOf(Species s);
const Creature *CreatureAt(int index);

/* The nearest creature matching a species mask, or NULL. */
const Creature *CreaturesNearest(unsigned int mask, Vector2 from, float range);

/* The nearest thing `hunter` would eat, and the nearest thing that would
   eat `prey`. These two are the whole food web at runtime. */
const Creature *CreaturesNearestPrey(Species hunter, Vector2 from, float range);
const Creature *CreaturesNearestThreat(Species prey, Vector2 from, float range);

/* The nearest thing `hunter` could eat *and* is already close enough to
   take hold of, respecting each creature's own published reach. */
const Creature *CreaturesCatchable(Species hunter, Vector2 from);

/* How a module is told one of its animals has been eaten. Registered
   once, at reset; without one, a species simply cannot be consumed. */
typedef void (*CreatureRemove)(int tag);

void CreaturesOnRemove(Species s, CreatureRemove fn);

/* Eats it: applies nothing itself, only removes it from the world.
   Returns false when nobody is listening for that species. */
bool CreaturesConsume(const Creature *c);

#endif /* ENTITY_CREATURES_H */
