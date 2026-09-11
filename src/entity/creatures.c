#include "entity/creatures.h"

#include <stddef.h>

/* Two buffers: one being written this tick, one being read. See the
   header on why reads are a tick behind. */
static Creature sBuf[2][CREATURES_MAX];
static int      sCount[2];
static int      sWrite;

static CreatureRemove sRemove[SPECIES_COUNT];

#define READ  (1 - sWrite)

void CreaturesReset(void)
{
    sCount[0] = 0;
    sCount[1] = 0;
    sWrite = 0;

    for (int i = 0; i < SPECIES_COUNT; i++) sRemove[i] = NULL;
}

void CreaturesBeginTick(void)
{
    sWrite = READ;
    sCount[sWrite] = 0;
}

void CreaturesPublish(Species s, Vector2 pos, int tag,
                      float reach, float noise)
{
    if (sCount[sWrite] >= CREATURES_MAX) return;

    const SpeciesTraits *t = SpeciesOf(s);

    sBuf[sWrite][sCount[sWrite]++] = (Creature){
        .species = s,
        .pos = pos,
        .width = t->width,
        .height = t->height,
        .tag = tag,
        .reach = reach,
        .noise = noise,
    };
}

int CreaturesCount(void)
{
    int n = 0;
    for (int i = 0; i < sCount[READ]; i++)
        if (!sBuf[READ][i].consumed) n++;
    return n;
}

int CreaturesCountOf(Species s)
{
    int n = 0;

    for (int i = 0; i < sCount[READ]; i++)
    {
        if (!sBuf[READ][i].consumed && sBuf[READ][i].species == s) n++;
    }

    return n;
}

const Creature *CreatureAt(int index)
{
    if (index < 0) return NULL;
    for (int i = 0; i < sCount[READ]; i++)
    {
        if (sBuf[READ][i].consumed) continue;
        if (index-- == 0) return &sBuf[READ][i];
    }
    return NULL;
}

static float DistanceSquared(Vector2 a, Vector2 b)
{
    float dx = a.x - b.x;
    float dy = a.y - b.y;

    return dx * dx + dy * dy;
}

const Creature *CreaturesNearest(unsigned int mask, Vector2 from, float range)
{
    if (range < 0.0f) return NULL;
    const Creature *best = NULL;
    float bestDistance = 0.0f;

    for (int i = 0; i < sCount[READ]; i++)
    {
        const Creature *c = &sBuf[READ][i];

        if (c->consumed) continue;
        if (!(mask & SPECIES_BIT(c->species))) continue;

        float d = DistanceSquared(c->pos, from);

        if (d > range * range) continue;
        if (best && d >= bestDistance) continue;

        best = c;
        bestDistance = d;
    }

    return best;
}

const Creature *CreaturesNearestPrey(Species hunter, Vector2 from, float range)
{
    return CreaturesNearest(SpeciesPreyOf(hunter), from, range);
}

const Creature *CreaturesNearestThreat(Species prey, Vector2 from, float range)
{
    return CreaturesNearest(SpeciesPredatorsOf(prey), from, range);
}

const Creature *CreaturesCatchable(Species hunter, Vector2 from)
{
    unsigned int menu = SpeciesPreyOf(hunter);

    const Creature *best = NULL;
    float bestDistance = 0.0f;

    for (int i = 0; i < sCount[READ]; i++)
    {
        const Creature *c = &sBuf[READ][i];

        if (c->consumed) continue;
        if (!(menu & SPECIES_BIT(c->species))) continue;
        if (c->reach <= 0.0f) continue;

        float d = DistanceSquared(c->pos, from);

        if (d > c->reach * c->reach) continue;
        if (best && d >= bestDistance) continue;

        best = c;
        bestDistance = d;
    }

    return best;
}

void CreaturesOnRemove(Species s, CreatureRemove fn)
{
    if (s < 0 || s >= SPECIES_COUNT) return;

    sRemove[s] = fn;
}

bool CreaturesConsume(const Creature *c)
{
    if (!c || c->consumed) return false;
    if (c->species < 0 || c->species >= SPECIES_COUNT) return false;
    if (!sRemove[c->species]) return false;

    /* Do not compact: callers may still hold a pointer into the census. */
    for (int b = 0; b < 2; b++)
    {
        for (int i = 0; i < sCount[b]; i++)
        {
            Creature *entry = &sBuf[b][i];
            if (entry->species == c->species && entry->tag == c->tag)
                entry->consumed = true;
        }
    }
    sRemove[c->species](c->tag);

    return true;
}
