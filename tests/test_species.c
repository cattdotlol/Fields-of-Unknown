/* The food web is a table, and a table is exactly the kind of thing that
   goes quietly wrong as it grows. These are the claims the game makes
   about who eats what, checked against the table rather than against a
   comment. */

#include "tests.h"

#include "entity/species.h"

#include <stdio.h>
#include <string.h>

static void TestEveryoneIsDescribed(void)
{
    puts("species");

    int named = 0;
    int sized = 0;

    for (int s = 0; s < SPECIES_COUNT; s++)
    {
        const SpeciesTraits *t = SpeciesOf((Species)s);

        if (t->name && strlen(t->name) > 0) named++;
        if (t->width > 0.0f && t->height > 0.0f && t->mass > 0.0f) sized++;
    }

    Check("every species has a name", named == SPECIES_COUNT, true);
    Check("and a body", sized == SPECIES_COUNT, true);

    /* Somewhere to be. A species in neither medium exists nowhere. */
    int placed = 0;
    for (int s = 0; s < SPECIES_COUNT; s++)
    {
        if (SpeciesLivesIn((Species)s, MEDIUM_LAND) ||
            SpeciesLivesIn((Species)s, MEDIUM_WATER)) placed++;
    }

    Check("and somewhere to live", placed == SPECIES_COUNT, true);
}

static void TestNothingEatsAboveItsWeight(void)
{
    /* The real check: every edge in the web is one the eater could
       actually make - smaller than itself, reachable in a medium it can
       enter, and worth eating. This is what stops the tenth animal
       arriving with a diet nobody noticed was impossible. */
    Check("the whole web holds up", SpeciesWebIsSane(), true);
}

static void TestTheCatIsNotTheTopOfIt(void)
{
    unsigned int hunters = SpeciesPredatorsOf(SPECIES_CAT);

    Check("something eats the cat", hunters != 0u, true);
    Check("on land, the stalker",
          (hunters & SPECIES_BIT(SPECIES_STALKER)) != 0u, true);
    Check("in water, the shark",
          (hunters & SPECIES_BIT(SPECIES_SHARK)) != 0u, true);

    /* And it is not the bottom either, or there would be no game. */
    Check("and the cat eats something", SpeciesPreyOf(SPECIES_CAT) != 0u, true);
}

static void TestTheSeaIsWorthTheTrip(void)
{
    /* Diving costs breath, warmth and the risk of a shark. If nothing
       down there fed the cat better than standing on a roof does, the
       whole ocean would be a hazard with no reason to enter it. */
    int water = 0;

    for (int s = 0; s < SPECIES_COUNT; s++)
    {
        if (!SpeciesEats(SPECIES_CAT, (Species)s)) continue;
        if (!SpeciesLivesIn((Species)s, MEDIUM_WATER)) continue;

        water++;
    }

    printf("    %d of the cat's meals live underwater\n", water);
    Check("there is food in the water", water > 0, true);

    /* A jellyfish is a lure, not a meal: bright, harmless, easy, and
       barely worth the breath. That gap is the lesson. */
    float jelly = SpeciesOf(SPECIES_JELLY)->food.hunger;
    float fish  = SpeciesOf(SPECIES_FISH)->food.hunger;

    printf("    a jellyfish is worth %.2f, a fish %.2f\n",
           (double)jelly, (double)fish);

    Check("a jellyfish can be eaten", SpeciesEdible(SPECIES_JELLY), true);
    Check("and is barely worth it", jelly < fish * 0.25f, true);
}

static void TestNothingEatsTheApexes(void)
{
    Check("nothing eats the stalker",
          SpeciesPredatorsOf(SPECIES_STALKER) == 0u, true);
    Check("nothing eats the shark",
          SpeciesPredatorsOf(SPECIES_SHARK) == 0u, true);

    /* The whale is not an apex predator, it just has no predators - it
       eats nothing in this list either. */
    Check("and the whale eats nothing here",
          SpeciesPreyOf(SPECIES_WHALE) == 0u, true);
}

void SuiteSpecies(void)
{
    TestEveryoneIsDescribed();
    TestNothingEatsAboveItsWeight();
    TestTheCatIsNotTheTopOfIt();
    TestTheSeaIsWorthTheTrip();
    TestNothingEatsTheApexes();
}
