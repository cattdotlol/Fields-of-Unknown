#include "core/rng.h"

/* A plain linear congruential generator, with the constants from
   Numerical Recipes. Statistical quality does not matter for anything it
   is asked to decide - which way a rat turns, how long it stands still -
   and being reproducible from a seed in two lines matters a great deal.
   These are the exact constants the entity modules each used privately
   before this existed, so seeded sequences are unchanged. */

void RngSeed(Rng *r, unsigned int seed)
{
    r->state = seed;
}

float Rng01(Rng *r)
{
    r->state = r->state * 1664525u + 1013904223u;

    /* The high bits of an LCG are the only ones worth having; the low
       ones cycle far too regularly to use. */
    return (float)((r->state >> 8) & 0xFFFFu) / 65535.0f;
}

float RngFine(Rng *r)
{
    r->state = r->state * 1664525u + 1013904223u;

    return (float)((r->state >> 8) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

float RngBetween(Rng *r, float lo, float hi)
{
    return lo + Rng01(r) * (hi - lo);
}

float RngFineBetween(Rng *r, float lo, float hi)
{
    return lo + RngFine(r) * (hi - lo);
}

bool RngChance(Rng *r, float probability)
{
    return Rng01(r) < probability;
}

float RngSign(Rng *r)
{
    return (Rng01(r) < 0.5f) ? -1.0f : 1.0f;
}

float RngSigned(Rng *r)
{
    return Rng01(r) * 2.0f - 1.0f;
}

int RngBelow(Rng *r, int count)
{
    if (count <= 0) return 0;

    int i = (int)(Rng01(r) * (float)count);

    /* Rng01 can return exactly 1.0, which would land one past the end. */
    return (i >= count) ? count - 1 : i;
}

int RngWeighted(Rng *r, const float *weights, int count)
{
    float total = 0.0f;

    for (int i = 0; i < count; i++)
    {
        if (weights[i] > 0.0f) total += weights[i];
    }

    if (total <= 0.0f) return -1;

    float roll = Rng01(r) * total;

    for (int i = 0; i < count; i++)
    {
        if (weights[i] <= 0.0f) continue;

        roll -= weights[i];
        if (roll <= 0.0f) return i;
    }

    /* Only reachable through rounding at the very top of the range. */
    for (int i = count - 1; i >= 0; i--)
    {
        if (weights[i] > 0.0f) return i;
    }

    return -1;
}
