#include "world/tree.h"

#include "world/weather.h"

#include <math.h>

typedef struct Species {
    float spread;        /* how far a side branch turns off its parent  */
    float leaderRatio;   /* the continuing branch keeps most of it      */
    float sideRatio;     /* laterals are much shorter                   */
    int   depth;
    int   children;
    float gravity;       /* bend back toward vertical, per generation   */
    float wobble;
} Species;

static const Species SPECIES[TREE_SPECIES_COUNT] = {
    /* A tall leader with short laterals is what fills a canopy out. The
       two ratios being one number is what made every tree a lollipop.

       Negative gravity is the interesting one: instead of easing back
       toward vertical, branches are pushed further from it every
       generation, which is what makes a willow weep.

       spread  leader  side  depth  kids  gravity  wobble */
    {  0.90f,  0.78f,  0.76f,    6,    2,   0.04f,  0.22f },   /* oak      */
    {  0.95f,  0.88f,  0.46f,    6,    2,   0.14f,  0.08f },   /* pine     */
    {  0.40f,  0.90f,  0.50f,    6,    2,   0.22f,  0.06f },   /* poplar   */
    {  0.62f,  0.86f,  0.58f,    6,    2,   0.10f,  0.20f },   /* birch    */
    {  0.72f,  0.72f,  0.82f,    6,    2,  -0.30f,  0.26f },   /* willow   */
    {  0.34f,  0.92f,  0.40f,    6,    2,   0.26f,  0.05f },   /* cypress  */
    {  1.05f,  0.70f,  0.80f,    5,    2,  -0.08f,  0.34f },   /* mangrove */
    {  1.15f,  0.74f,  0.74f,    5,    2,  -0.06f,  0.50f },   /* gnarled  */
};

static unsigned int Hash(unsigned int a, unsigned int b)
{
    unsigned int h = a * 2654435761u ^ (b + 0x9E3779B9u) * 2246822519u;
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

/* Deterministic per branch: the path taken to reach it is the seed, so a
   tree regrows identically without storing anything. */
static float Noise(unsigned int variant, unsigned int path, unsigned int salt)
{
    return (float)(Hash(variant ^ path, salt) & 0xFFFFu) / 65535.0f - 0.5f;
}

const char *TreeSpeciesName(TreeSpecies species)
{
    switch (species)
    {
        case TREE_OAK:      return "OAK";
        case TREE_PINE:     return "PINE";
        case TREE_POPLAR:   return "POPLAR";
        case TREE_BIRCH:    return "BIRCH";
        case TREE_WILLOW:   return "WILLOW";
        case TREE_CYPRESS:  return "CYPRESS";
        case TREE_MANGROVE: return "MANGROVE";
        default:            return "DEAD";
    }
}

/* Species follows the ground it is standing on, the way it does in life:
   willow, cypress and mangrove want their feet wet, pine and birch want
   drainage, oak and poplar take the middle. */
TreeSpecies TreeSpeciesOf(const Tree *tree)
{
    if (tree->dead) return TREE_GNARLED;

    /* Measured against the fair-weather water line, not the flood peak:
       a tree can't change species when the water rises. 0 is high ground,
       1 is the shoreline. */
    float dry = WeatherBaseWaterY();
    float wet = (tree->baseY - (dry - 64.0f)) / 64.0f;

    if (wet < 0.0f) wet = 0.0f;
    if (wet > 1.0f) wet = 1.0f;

    unsigned int roll = Hash(tree->variant, 77u) % 100u;

    if (wet > 0.68f)
    {
        /* Standing water. Only these three manage it. */
        if (roll < 40u) return TREE_MANGROVE;
        if (roll < 74u) return TREE_WILLOW;
        return TREE_CYPRESS;
    }

    if (wet > 0.38f)
    {
        /* Damp ground - a riverbank, not a swamp. */
        if (roll < 34u) return TREE_WILLOW;
        if (roll < 66u) return TREE_OAK;
        return TREE_POPLAR;
    }

    /* Dry and drained. */
    if (roll < 34u) return TREE_PINE;
    if (roll < 62u) return TREE_BIRCH;
    if (roll < 86u) return TREE_OAK;
    return TREE_POPLAR;
}

Color TreeBarkColour(TreeSpecies species, int depth)
{
    switch (species)
    {
        case TREE_BIRCH:
            return (depth < 2) ? (Color){ 176, 174, 166, 255 }
                               : (Color){ 132, 130, 124, 255 };

        case TREE_MANGROVE:
            return (depth < 2) ? (Color){ 52, 32, 30, 255 }
                               : (Color){ 38, 24, 24, 255 };

        case TREE_PINE:
        case TREE_CYPRESS:
            return (depth < 2) ? (Color){ 44, 32, 28, 255 }
                               : (Color){ 32, 24, 22, 255 };

        default:
            return (depth < 2) ? (Color){ 30, 26, 30, 255 }
                               : (Color){ 19, 17, 20, 255 };
    }
}

typedef struct Builder {
    TreeBranch *out;
    int count;
    int max;
    unsigned int variant;
    const Species *species;
} Builder;

static void Grow(Builder *b, Vector2 from, float angle, float length,
                 float thickness, int depth, unsigned int path)
{
    if (b->count >= b->max || depth > b->species->depth || length < 3.0f) return;

    /* Branches curve rather than running straight, so the segment ends a
       little off the angle it started at. */
    float curve = Noise(b->variant, path, 5u) * b->species->wobble;
    float endAngle = angle + curve;

    Vector2 to = {
        from.x + sinf(endAngle) * length,
        from.y - cosf(endAngle) * length,
    };

    /* Leaves on the last two generations, not just the very ends: one
       ring of clusters reads as a lollipop rather than a canopy. */
    bool tip = (depth >= b->species->depth - 1);

    b->out[b->count].from = from;
    b->out[b->count].to = to;
    b->out[b->count].thickness = thickness;
    b->out[b->count].depth = depth;
    b->out[b->count].tip = tip;
    b->count++;

    if (tip) return;

    int kids = b->species->children;
    if (kids < 2) kids = 2;

    /* Da Vinci: the children together carry the parent's cross-section. */
    float childThickness = thickness / sqrtf((float)kids);

    for (int i = 0; i < kids; i++)
    {
        unsigned int childPath = path * 4u + (unsigned int)i + 1u;

        /* Apical dominance: child 0 carries on, the rest turn away. */
        bool leader = (i == 0);

        /* Alternate on the path, not on the child index. Keying it to the
           index meant child 0 was always the leader and child 1 always
           turned the same way, so every fork bent right and the tree came
           out as a broom. */
        float side = (((path + (unsigned int)i) & 1u) != 0) ? 1.0f : -1.0f;
        float turn = leader ? (Noise(b->variant, childPath, 9u) * 0.20f)
                            : (side * b->species->spread *
                               (0.75f + Noise(b->variant, childPath, 11u) * 0.5f));

        /* Gravitropism: ease back toward vertical, not snap to it. */
        float childAngle = endAngle + turn;
        childAngle -= childAngle * b->species->gravity;

        float childLength = length *
                            (leader ? b->species->leaderRatio : b->species->sideRatio) *
                            (0.85f + Noise(b->variant, childPath, 13u) * 0.3f);

        Grow(b, to, childAngle, childLength, childThickness, depth + 1, childPath);
    }
}

int TreeBuild(const Tree *tree, TreeBranch *out, int max)
{
    TreeSpecies species = TreeSpeciesOf(tree);

    Builder b = { out, 0, max, tree->variant, &SPECIES[species] };

    /* The trunk leans a little, and thickness scales with the tree. */
    float lean = Noise(tree->variant, 1u, 3u) * 0.18f;
    float trunk = tree->height * 0.30f;
    float thickness = 3.0f + tree->height * 0.055f;

    /* Mangroves are held clear of the water on stilt roots, which is the
       whole reason they can live in it. */
    if (species == TREE_MANGROVE)
    {
        float lift = 26.0f + tree->height * 0.10f;
        Vector2 crown = { tree->x, tree->baseY - lift };

        for (int i = 0; i < 4 && b.count < b.max; i++)
        {
            float side = ((i & 1) != 0) ? 1.0f : -1.0f;
            float reach = (14.0f + (float)(i / 2) * 12.0f) * side;

            b.out[b.count].from = crown;
            b.out[b.count].to = (Vector2){ tree->x + reach, tree->baseY };
            b.out[b.count].thickness = thickness * 0.55f;
            b.out[b.count].depth = 0;
            b.out[b.count].tip = false;
            b.count++;
        }

        Grow(&b, crown, lean, trunk, thickness, 0, 1u);
        return b.count;
    }

    Grow(&b, (Vector2){ tree->x, tree->baseY }, lean, trunk, thickness, 0, 1u);

    return b.count;
}
