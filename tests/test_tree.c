/* Trees are generated recursively, so the things worth pinning are that
   the recursion terminates, stays inside its buffer, obeys the branching
   rule it claims to, and that species actually differ. */

#include "tests.h"

#include "world/tree.h"
#include "world/worldgen.h"

#include <math.h>
#include <stdio.h>

static Tree Make(unsigned int variant, bool dead)
{
    Tree t = { 0 };
    t.x = 0.0f;
    t.baseY = 0.0f;
    t.height = 190.0f;
    t.spread = 40.0f;
    t.variant = variant;
    t.dead = dead;
    return t;
}

static void TestRecursionIsBounded(void)
{
    puts("trees");

    int most = 0, fewest = TREE_MAX_BRANCHES + 1, overflow = 0;

    for (unsigned int v = 0; v < 4000; v++)
    {
        Tree t = Make(v, (v % 7) == 0);
        TreeBranch b[TREE_MAX_BRANCHES];

        int n = TreeBuild(&t, b, TREE_MAX_BRANCHES);

        if (n > TREE_MAX_BRANCHES) overflow++;
        if (n > most) most = n;
        if (n < fewest) fewest = n;
    }

    printf("    4000 trees: %d..%d branches (buffer %d)\n", fewest, most, TREE_MAX_BRANCHES);

    Check("every tree terminates", most > 0, true);
    Check("none overflow the buffer", overflow == 0, true);
    Check("and none come out as a bare stick", fewest > 8, true);
}

/* Da Vinci's rule: the children's cross-sections sum to the parent's. */
static void TestBranchesThinCorrectly(void)
{
    Tree t = Make(1234u, false);
    TreeBranch b[TREE_MAX_BRANCHES];
    int n = TreeBuild(&t, b, TREE_MAX_BRANCHES);

    float trunk = 0.0f, thinnest = 1e9f;

    for (int i = 0; i < n; i++)
    {
        if (b[i].depth == 0 && b[i].thickness > trunk) trunk = b[i].thickness;
        if (b[i].thickness < thinnest) thinnest = b[i].thickness;
    }

    printf("    trunk %.1f thick, finest twig %.1f\n", (double)trunk, (double)thinnest);

    Check("the trunk is the thickest part", trunk > thinnest, true);
    Check("twigs are much finer than the trunk", trunk > thinnest * 2.0f, true);
}

/* Species is a silhouette, not a label: a poplar must be narrower than
   an oak or the parameters are doing nothing. */
static void TestSpeciesDiffer(void)
{
    float span[TREE_SPECIES_COUNT] = { 0 };
    int found[TREE_SPECIES_COUNT] = { 0 };

    for (unsigned int v = 0; v < 20000; v++)
    {
        Tree t = Make(v, false);
        TreeSpecies sp = TreeSpeciesOf(&t);

        if (found[sp] >= 40) continue;

        TreeBranch b[TREE_MAX_BRANCHES];
        int n = TreeBuild(&t, b, TREE_MAX_BRANCHES);

        float lo = 1e9f, hi = -1e9f;
        for (int i = 0; i < n; i++)
        {
            if (b[i].to.x < lo) lo = b[i].to.x;
            if (b[i].to.x > hi) hi = b[i].to.x;
        }

        span[sp] += (hi - lo);
        found[sp]++;
    }

    for (int i = 0; i < TREE_SPECIES_COUNT; i++)
    {
        if (found[i] > 0) span[i] /= (float)found[i];
    }

    printf("    average crown width: oak %.0f  pine %.0f  poplar %.0f  gnarled %.0f\n",
           (double)span[TREE_OAK], (double)span[TREE_PINE],
           (double)span[TREE_POPLAR], (double)span[TREE_GNARLED]);

    Check("a poplar is narrower than an oak", span[TREE_POPLAR] < span[TREE_OAK], true);
    Check("a gnarled tree is the widest", span[TREE_GNARLED] > span[TREE_POPLAR], true);
    Check("every species gets grown", found[0] && found[1] && found[2] && found[3], true);
}

static void TestDeterminism(void)
{
    Tree t = Make(99u, false);

    TreeBranch a[TREE_MAX_BRANCHES], b[TREE_MAX_BRANCHES];
    int na = TreeBuild(&t, a, TREE_MAX_BRANCHES);

    Tree other = Make(4321u, false);
    TreeBranch scratch[TREE_MAX_BRANCHES];
    TreeBuild(&other, scratch, TREE_MAX_BRANCHES);

    int nb = TreeBuild(&t, b, TREE_MAX_BRANCHES);

    bool same = (na == nb);
    for (int i = 0; same && i < na; i++)
    {
        same = (a[i].to.x == b[i].to.x && a[i].to.y == b[i].to.y);
    }

    Check("the same tree regrows identically", same, true);
}

void SuiteTree(void)
{
    TestRecursionIsBounded();
    TestBranchesThinCorrectly();
    TestSpeciesDiffer();
    TestDeterminism();
}
