#ifndef CORE_RNG_H
#define CORE_RNG_H

#include <stdbool.h>

/* One deterministic stream of numbers.

   Every module that needs randomness owns a stream of its own rather than
   drawing from a shared one. That is not tidiness. World generation has
   to be a pure function of (seed, chunk index) or the world changes shape
   when you walk back through it - so a rat deciding which way to turn
   must not be able to advance the same counter the terrain is reading
   from. Keeping the state in a struct the caller holds makes sharing it
   by accident impossible. */

typedef struct Rng {
    unsigned int state;
} Rng;

void RngSeed(Rng *r, unsigned int seed);

float Rng01(Rng *r);                            /* 0..1 inclusive       */
float RngBetween(Rng *r, float lo, float hi);
bool  RngChance(Rng *r, float probability);     /* true that often      */
float RngSign(Rng *r);                          /* -1 or 1, evenly      */
float RngSigned(Rng *r);                        /* -1..1, evenly        */
int   RngBelow(Rng *r, int count);              /* 0..count-1           */

/* The same stream read at 24 bits instead of 16.

   Which one to use is not a matter of taste. A value that becomes a
   decision - which way to turn, whether to bolt - wants Rng01: sixteen
   bits is far more resolution than the decision has. A value that becomes
   a distance wants this: world generation reads its numbers straight out
   as positions and widths, and the two precisions consume the stream
   differently, so swapping one for the other silently reshapes every
   world grown from a given seed. */
float RngFine(Rng *r);
float RngFineBetween(Rng *r, float lo, float hi);

/* Picks one of `count` weighted options, or -1 if every weight is zero.
   Used wherever a table decides how often something happens rather than
   a chain of hand-written thresholds. */
int RngWeighted(Rng *r, const float *weights, int count);

#endif /* CORE_RNG_H */
