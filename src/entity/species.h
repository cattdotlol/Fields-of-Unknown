#ifndef ENTITY_SPECIES_H
#define ENTITY_SPECIES_H

#include "raylib.h"

#include <stdbool.h>

/* Every living thing in one list, and one table of what each of them is.

   Before this, a rat's size lived in rat.c, what a rat was worth to eat
   lived in the gameplay screen, and nothing anywhere said that a stalker
   would happily eat one. Facts about a species that more than one module
   needs belong in one place, stated once - otherwise the fourth animal
   costs as much to add as the first three did, and by the tenth nobody
   can answer "what eats what" without reading every file.

   This is the data half. What a creature *does* stays in its own module,
   because a rat's nerve and a jellyfish's pulse have nothing in common
   and pretending otherwise would buy nothing. */

typedef enum Species {
    SPECIES_CAT = 0,
    SPECIES_RAT,
    SPECIES_STALKER,
    SPECIES_JELLY,
    SPECIES_SHARK,
    SPECIES_WHALE,
    SPECIES_FISH,
    SPECIES_COUNT
} Species;

/* Species fit in a 32-bit mask, which is what the diets below are. */
#define SPECIES_BIT(s) (1u << (unsigned int)(s))

/* Where a thing can be. The cat is the only one that is both, which is
   most of what makes the water interesting: it is the only route to the
   things that live down there, and the only escape from the things that
   do not. */
typedef enum Medium {
    MEDIUM_LAND  = 1u << 0,
    MEDIUM_WATER = 1u << 1,
} Medium;

/* What eating one does. The same triple VitalsApply takes, so a meal is
   a number in this table rather than a literal at the call site. */
typedef struct Nutrition {
    float hunger;
    float health;
    float warmth;
} Nutrition;

typedef struct SpeciesTraits {
    const char   *name;
    unsigned char medium;

    float width;
    float height;

    /* Rough ordering by size. Nothing eats something bigger than itself,
       which is checked rather than assumed: it is what keeps the diets
       below honest as they grow. */
    float mass;

    /* Who it will eat, as a mask of SPECIES_BIT. This is the food web,
       written down in one place. */
    unsigned int diet;

    /* What it is worth to whatever eats it. Zero hunger means nothing
       gets fed by it, which is how a thing is inedible without needing a
       separate flag to fall out of step with this one. */
    Nutrition food;
} SpeciesTraits;

const SpeciesTraits *SpeciesOf(Species s);
const char          *SpeciesName(Species s);

/* The food web, asked one edge at a time. */
bool SpeciesEats(Species eater, Species prey);
bool SpeciesEdible(Species s);

/* Everything that would eat this one, as a mask. What a prey animal has
   to be afraid of, without prey animals each keeping their own list. */
unsigned int SpeciesPredatorsOf(Species s);

/* Everything this one would eat, as a mask. */
unsigned int SpeciesPreyOf(Species s);

bool SpeciesLivesIn(Species s, Medium medium);

/* True when every diet in the table is something the eater could
   plausibly catch and swallow - smaller than itself, reachable in a
   medium it can enter, and edible at all. Not used by the game; the
   tests call it, so that adding an animal cannot quietly introduce a
   rat that eats whales. */
bool SpeciesWebIsSane(void);

#endif /* ENTITY_SPECIES_H */
