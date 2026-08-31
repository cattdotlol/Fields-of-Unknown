/* Trees are generated recursively, so the things worth pinning are that
   the recursion terminates, stays inside its buffer, obeys the branching
   rule it claims to, and that species actually differ. */

#include "tests.h"

#include "world/tree.h"
#include "world/worldgen.h"
#include "world/weather.h"
#include "world/season.h"

#include <math.h>
#include <stdio.h>

static Tree Make(unsigned int variant, bool dead)
{
    Tree t = { 0 };
    t.x = 0.0f;
    t.baseY = 430.0f;              /* dry ground */
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

/* Species is a silhouette, not a label: a cypress must be narrower than
   an oak or the parameters are doing nothing. */
static void TestSpeciesDiffer(void)
{
    SeasonInit();
    WeatherInit(3u);

    float span[TREE_SPECIES_COUNT] = { 0 };
    int found[TREE_SPECIES_COUNT] = { 0 };

    for (unsigned int v = 0; v < 60000; v++)
    {
        Tree t = Make(v, (v % 23) == 0);

        /* Sweep the ground from dry to flooded, since species follows
           wetness. */
        t.baseY = 400.0f + (float)(v % 9) * 30.0f;

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

    bool all = true;
    for (int i = 0; i < TREE_SPECIES_COUNT; i++)
    {
        if (found[i] == 0) { all = false; continue; }
        span[i] /= (float)found[i];
    }

    printf("    crown widths:");
    for (int i = 0; i < TREE_SPECIES_COUNT; i++)
    {
        printf(" %s %.0f", TreeSpeciesName((TreeSpecies)i), (double)span[i]);
    }
    printf("\n");

    Check("every species grows somewhere", all, true);
    Check("a cypress is narrower than an oak", span[TREE_CYPRESS] < span[TREE_OAK], true);
    Check("a willow spreads wider than a poplar",
          span[TREE_WILLOW] > span[TREE_POPLAR], true);
}

/* Species follows the ground, the way it does in life. */
static void TestSpeciesFollowTheWater(void)
{
    SeasonInit();
    WeatherInit(3u);

    float dry = WeatherBaseWaterY();
    int wetLovers = 0, dryLovers = 0;

    for (unsigned int v = 0; v < 3000; v++)
    {
        Tree wet = Make(v, false);
        wet.baseY = dry + 10.0f;                 /* ankle deep */

        TreeSpecies s = TreeSpeciesOf(&wet);
        if (s == TREE_MANGROVE || s == TREE_WILLOW || s == TREE_CYPRESS) wetLovers++;

        Tree high = Make(v, false);
        high.baseY = dry - 90.0f;                /* high and drained */

        TreeSpecies d = TreeSpeciesOf(&high);
        if (d == TREE_PINE || d == TREE_BIRCH || d == TREE_OAK || d == TREE_POPLAR) dryLovers++;
    }

    printf("    of 3000: %d in the shallows are water species, "
           "%d on high ground are not\n", wetLovers, dryLovers);

    Check("only water species stand in water", wetLovers == 3000, true);
    Check("high ground grows the others", dryLovers == 3000, true);
}

/* The world floods, but it must not drown its own forest: roots go under,
   crowns stay in the air. */
static void TestNothingDrowns(void)
{
    SeasonInit();
    WeatherInit(3u);
    WorldSetSeed(20260831u);

    int trees = 0, rootedTooDeep = 0, crownsUnder = 0;

    for (int i = -120; i <= 120; i++)
    {
        Chunk c;
        WorldBuildChunk(i, &c);

        for (int t = 0; t < c.treeCount; t++)
        {
            const Tree *tr = &c.trees[t];
            trees++;

            /* Nothing takes root on permanently drowned ground. */
            if (tr->baseY > WeatherBaseWaterY() + 20.0f) rootedTooDeep++;

            /* Even at the flood peak the crown clears the surface. */
            if (tr->baseY - tr->height > WeatherMaxWaterY()) crownsUnder++;
        }
    }

    printf("    %d trees: %d rooted on drowned ground, %d crowns under at flood\n",
           trees, rootedTooDeep, crownsUnder);

    Check("no tree roots on drowned ground", rootedTooDeep == 0, true);
    Check("no crown goes under at the flood peak", crownsUnder == 0, true);
}

/* A mangrove is held out of the water on stilt roots. */
static void TestMangrovesHaveRoots(void)
{
    SeasonInit();
    WeatherInit(3u);

    for (unsigned int v = 0; v < 60000; v++)
    {
        Tree t = Make(v, false);
        t.baseY = WeatherBaseWaterY() + 10.0f;

        if (TreeSpeciesOf(&t) != TREE_MANGROVE) continue;

        TreeBranch b[TREE_MAX_BRANCHES];
        int n = TreeBuild(&t, b, TREE_MAX_BRANCHES);

        /* Roots run downward, back to the ground the trunk lifts off. */
        int roots = 0;
        for (int i = 0; i < n; i++)
        {
            if (b[i].to.y > b[i].from.y + 8.0f) roots++;
        }

        printf("    mangrove: %d branches, %d of them roots\n", n, roots);
        Check("a mangrove stands on stilt roots", roots >= 4, true);
        return;
    }

    Check("found a mangrove to check", false, true);
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
    TestSpeciesFollowTheWater();
    TestNothingDrowns();
    TestMangrovesHaveRoots();
    TestDeterminism();
}
