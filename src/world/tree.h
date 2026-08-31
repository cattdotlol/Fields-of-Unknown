#ifndef WORLD_TREE_H
#define WORLD_TREE_H

#include "raylib.h"
#include "world/worldgen.h"

/* Trees built from how trees actually grow, rather than a trunk with
   sticks glued on.

   The rules that matter:

   - Recursive branching. A branch splits, its children split, and so on
     for a few generations. That self-similarity is what reads as a tree.
   - Da Vinci's rule: the cross-sectional area of a branch equals the sum
     of its children's, so a child's thickness is the parent's over the
     square root of the number of children. Trees really do obey this.
   - Apical dominance: one child continues the parent's direction and
     keeps most of its length; the others diverge and are shorter. Without
     it every fork is symmetrical and it looks like a diagram.
   - Gravitropism: branches bend back toward vertical as they grow, so a
     canopy curls upward instead of drooping outward.
   - Species is really just a pair of numbers - branch angle and length
     ratio. Narrow and short gives a spruce; wide and long gives an oak. */

#define TREE_MAX_BRANCHES 128

typedef enum TreeSpecies {
    TREE_OAK = 0,       /* wide, spreading, heavy canopy */
    TREE_PINE,          /* narrow, conical, tiered        */
    TREE_POPLAR,        /* columnar, near-vertical        */
    TREE_GNARLED,       /* alien, twisted, sparse         */
    TREE_SPECIES_COUNT
} TreeSpecies;

typedef struct TreeBranch {
    Vector2 from;
    Vector2 to;
    float   thickness;
    int     depth;          /* 0 is the trunk */
    bool    tip;            /* carries leaves */
} TreeBranch;

TreeSpecies TreeSpeciesOf(const Tree *tree);

/* Fills `out` with the whole tree and returns how many branches it used.
   Deterministic: the same tree always grows the same way. */
int TreeBuild(const Tree *tree, TreeBranch *out, int max);

#endif /* WORLD_TREE_H */
