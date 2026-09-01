#include "entity/species.h"

/* Sizes are the collision box each module already used, moved here so
   they stop being three private opinions about how big a rat is.

   Mass is not kilograms. It is an ordering: the only thing it decides is
   who could conceivably eat whom, and SpeciesWebIsSane checks the diets
   below against it rather than trusting them.

   Designated initialisers on purpose. A positional table silently shifts
   every row the day a species is inserted in the middle, and the failure
   is a rat with a whale's diet rather than a compile error. */
static const SpeciesTraits TRAITS[SPECIES_COUNT] = {

    /* The only thing that lives in both mediums, which is the whole shape
       of the game: the water is the only way to reach what is down there
       and the only way out of what is up here. Eaten by the stalker - the
       cat is not the top of this. */
    [SPECIES_CAT] = {
        .name = "cat",
        .medium = MEDIUM_LAND | MEDIUM_WATER,
        .width = 22.0f, .height = 30.0f,
        .mass = 4.0f,
        .diet = SPECIES_BIT(SPECIES_RAT)
              | SPECIES_BIT(SPECIES_FISH)
              | SPECIES_BIT(SPECIES_JELLY),
        .food = { 0.60f, 0.10f, 0.15f },
    },

    /* The first real meal, and the only one that does not cost breath. */
    [SPECIES_RAT] = {
        .name = "rat",
        .medium = MEDIUM_LAND,
        .width = 16.0f, .height = 10.0f,
        .mass = 0.5f,
        .diet = 0u,
        .food = { 0.45f, 0.05f, 0.10f },
    },

    /* Apex on land. It hunts the cat by ear, and it will take a rat on
       the way past - which is the only reason to believe it eats at all
       rather than existing purely to chase you. */
    [SPECIES_STALKER] = {
        .name = "stalker",
        .medium = MEDIUM_LAND,
        .width = 40.0f, .height = 28.0f,
        .mass = 8.0f,
        .diet = SPECIES_BIT(SPECIES_CAT)
              | SPECIES_BIT(SPECIES_RAT),
        .food = { 0.0f, 0.0f, 0.0f },
    },

    /* Harmless, luminous, and almost entirely water. Easy to catch and
       worth nearly nothing, which is a lesson the player is allowed to
       learn by spending a lungful on one. */
    [SPECIES_JELLY] = {
        .name = "jellyfish",
        .medium = MEDIUM_WATER,
        .width = 14.0f, .height = 14.0f,
        .mass = 0.1f,
        .diet = 0u,
        .food = { 0.05f, 0.0f, 0.0f },
    },

    /* Apex in water, and faster than the cat swims. Its actual living is
       made on fish; the cat is opportunism. */
    [SPECIES_SHARK] = {
        .name = "shark",
        .medium = MEDIUM_WATER,
        .width = 34.0f, .height = 16.0f,
        .mass = 12.0f,
        .diet = SPECIES_BIT(SPECIES_FISH)
              | SPECIES_BIT(SPECIES_CAT),
        .food = { 0.0f, 0.0f, 0.0f },
    },

    /* Eats nothing in this list. It strains the water for things too
       small to be worth modelling, and it goes deeper than the cat can
       follow, so it is scenery on a scale nothing else here has. */
    [SPECIES_WHALE] = {
        .name = "whale",
        .medium = MEDIUM_WATER,
        .width = 120.0f, .height = 60.0f,
        .mass = 200.0f,
        .diet = 0u,
        .food = { 0.0f, 0.0f, 0.0f },
    },

    /* What the sea is actually for. Worth slightly less than a rat and
       found in numbers, so a good dive beats a good afternoon on land -
       if you can get back up. */
    [SPECIES_FISH] = {
        .name = "fish",
        .medium = MEDIUM_WATER,
        .width = 18.0f, .height = 10.0f,
        .mass = 0.3f,
        .diet = 0u,
        .food = { 0.40f, 0.04f, 0.0f },
    },
};

static bool Known(Species s)
{
    return (s >= 0 && s < SPECIES_COUNT);
}

const SpeciesTraits *SpeciesOf(Species s)
{
    /* The cat is the safe answer to a bad index: it is the one species
       guaranteed to exist, and a caller that got here is already wrong. */
    return Known(s) ? &TRAITS[s] : &TRAITS[SPECIES_CAT];
}

const char *SpeciesName(Species s)
{
    return Known(s) ? TRAITS[s].name : "?";
}

bool SpeciesEats(Species eater, Species prey)
{
    if (!Known(eater) || !Known(prey)) return false;

    return (TRAITS[eater].diet & SPECIES_BIT(prey)) != 0u;
}

bool SpeciesEdible(Species s)
{
    return Known(s) && TRAITS[s].food.hunger > 0.0f;
}

unsigned int SpeciesPreyOf(Species s)
{
    return Known(s) ? TRAITS[s].diet : 0u;
}

unsigned int SpeciesPredatorsOf(Species s)
{
    if (!Known(s)) return 0u;

    unsigned int mask = 0u;

    for (int i = 0; i < SPECIES_COUNT; i++)
    {
        if (TRAITS[i].diet & SPECIES_BIT(s)) mask |= SPECIES_BIT(i);
    }

    return mask;
}

bool SpeciesLivesIn(Species s, Medium medium)
{
    return Known(s) && (TRAITS[s].medium & (unsigned char)medium) != 0u;
}

bool SpeciesWebIsSane(void)
{
    for (int eater = 0; eater < SPECIES_COUNT; eater++)
    {
        for (int prey = 0; prey < SPECIES_COUNT; prey++)
        {
            if (!(TRAITS[eater].diet & SPECIES_BIT(prey))) continue;

            if (eater == prey) return false;                       /* itself */
            if (TRAITS[prey].mass >= TRAITS[eater].mass) return false;

            /* It has to be able to get at it: something that only walks
               cannot make a living on something that only swims. */
            if ((TRAITS[eater].medium & TRAITS[prey].medium) == 0u) return false;

            /* And there has to be something in it. A diet entry worth no
               hunger is a line nobody would ever act on. */
            if (TRAITS[prey].food.hunger <= 0.0f) return false;
        }
    }

    return true;
}
