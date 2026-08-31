/* Collision resolution used to push a body out along whatever direction
   its velocity implied. A body that was only slightly inside a solid came
   out fine; a body deeply inside one - which happens constantly, because
   swimming holds it at the waterline and the waterline sits at about
   ground height - was thrown clean across it. Up to 420 units backwards,
   which is most of a chunk. */

#include "tests.h"

#include "world/physics.h"
#include "world/terrain.h"
#include "world/worldgen.h"

#include <math.h>
#include <stdio.h>

/* A single wide slab, so "which edge did it leave by" is unambiguous. */
static void OneWideSlab(void)
{
    WorldSetSeed(1u);
    TerrainStream(0.0f);
}

static void TestNearestEdgeWins(void)
{
    puts("physics");

    /* Deep inside a 400-wide slab, close to its right-hand edge, drifting
       right. The old rule sent it to the left edge. */
    Rectangle slab = { 0.0f, 500.0f, 400.0f, 280.0f };

    Body b;
    BodyInit(&b, (Vector2){ 380.0f, 560.0f }, 22.0f, 30.0f);
    b.vel.x = 40.0f;

    float pushLeft  = (b.pos.x + 11.0f) - slab.x;
    float pushRight = (slab.x + slab.width) - (b.pos.x - 11.0f);

    printf("    body at x=380 inside a slab spanning 0..400\n");
    printf("    distance out to the left %.0f, to the right %.0f\n",
           (double)pushLeft, (double)pushRight);

    Check("the right edge really is nearer", pushRight < pushLeft, true);

    /* The rule the fix implements. */
    float resolved = (pushLeft < pushRight) ? (slab.x - 11.0f)
                                            : (slab.x + slab.width + 11.0f);

    printf("    resolves to x=%.0f\n", (double)resolved);

    Check("it leaves by the near edge", resolved > 380.0f, true);
    Check("it is not flung across the slab", fabsf(resolved - 380.0f) < 100.0f, true);
}

/* The same thing against the real world, which is what actually broke. */
static void TestNoLargeShovesInPlay(void)
{
    OneWideSlab();

    Body b;
    BodyInit(&b, WorldSpawnPoint(), 22.0f, 30.0f);

    int shoves = 0;
    float worst = 0.0f;

    for (int t = 0; t < 60 * 300; t++)
    {
        TerrainStream(b.pos.x);
        BodyBeginTick(&b);

        b.vel.x = 200.0f;
        if (b.grounded && (t % 37) == 0) b.vel.y = -600.0f;
        BodyApplyGravity(&b, 1900.0f, 1300.0f, 1.0f / 60.0f);

        /* Keep it out of the endless fall a channel would cause. */
        if (b.pos.y > 900.0f) { b.pos.y = 560.0f; b.vel.y = 0.0f; }

        float before = b.pos.x;
        BodyMove(&b, 1.0f / 60.0f);

        float moved = b.pos.x - before;
        if (moved < -0.01f)
        {
            shoves++;
            if (-moved > worst) worst = -moved;
        }
    }

    printf("    five minutes of running right: %d backward shoves, worst %.1f units\n",
           shoves, (double)worst);

    Check("nothing is thrown backwards across a solid", worst < 20.0f, true);
}

void SuitePhysics(void)
{
    TestNearestEdgeWins();
    TestNoLargeShovesInPlay();
}
