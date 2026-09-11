#include "entity/aquatic.h"
#include "core/rng.h"
#include "entity/creatures.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "world/daylight.h"
#include "world/ocean.h"
#include "world/terrain.h"
#include "world/weather.h"

#include <math.h>
#include <stddef.h>

/* The drowned city is a lid on the sea, and this is where it ends.

   Measured across four thousand units of map: above 240 down, better
   than nine tenths of the water is solid even to something jellyfish
   sized, and it does not open out properly until about 300. Only the
   channels get through, and only a cat fits those. That one fact
   decides the whole shape of what lives here - nothing out there is
   ever seen from the surface, and the danger is in diving rather than
   in swimming. */
#define SEA_TOP     300.0f

/* Clearance kept above the rock. The floor runs from 431 down to 2199,
   so without this a whale over the shelf would spend its life grinding
   along the bottom. */
#define FLOOR_CLEAR  70.0f

/* Where each kind lives, as depth below the surface, and how much of the
   daily migration it takes part in.

   The scattering layer rises after dark and sinks again at first light -
   the largest daily movement of animals anywhere. Jellyfish ride it
   hardest, sharks follow what it carries, and a whale ignores it
   entirely: it dives on its own clock, because it has to come back up
   to breathe. */
typedef struct Band {
    float shallow;
    float deep;
    float migrate;   /* 0 stays put, 1 follows the light the whole way */
    float homeLo;    /* where in the band an individual settles */
    float homeHi;
} Band;

static const Band BANDS[AQUA_KIND_COUNT] = {
    [AQUA_JELLY] = { SEA_TOP,         900.0f, 0.85f, 0.25f, 0.75f },
    /* Deep enough to reach the fish. Measured: with the old floor of 620
       a shark spent its life a hundred units above the school it was
       supposed to be living on, and never once closed. A predator's band
       is not its own preference, it is wherever its food is. */
    [AQUA_SHARK] = { SEA_TOP,         780.0f, 0.55f, 0.25f, 0.75f },
    /* A whale sits at the bottom of its own range rather than the middle
       of it, and needs more room under the city than the others. Its
       floor is past the cat's crush depth, so following one all the way
       down is not an option. */
    [AQUA_WHALE] = { SEA_TOP + 40.0f, 1800.0f, 0.00f, 0.75f, 1.00f },
    /* Fish ride the scattering layer harder than anything else: they are
       most of what it is made of. Spread wide through the band, so a dive
       finds a school at some depth rather than a single fish layer. */
    [AQUA_FISH]  = { SEA_TOP,          760.0f, 0.75f, 0.15f, 0.85f },
};

#define JELLY_MAX  8
#define SHARK_MAX  2
#define WHALE_MAX  1
#define FISH_MAX  24

#define JELLY_SPEED   26.0f
#define SHARK_CRUISE  70.0f
#define SHARK_CHARGE 205.0f      /* the cat swims at 74 */
#define WHALE_SPEED   34.0f

/* The same bargain a rat gets, transposed into water. A rat flees at 150
   against a cat that sprints at 200: it can be run down, and running is
   loud enough to bring something worse. A fish bolts at 70 against a cat
   that swims at 74 - so the chase is winnable, but every second of it is
   a second of air, and the breath is what the sea charges instead of
   noise. At 165 nothing in the water could ever be caught at all. */
#define FISH_CRUISE   34.0f
#define FISH_DART     70.0f

/* Schooling. Three rules and nothing else: hold with the ones near you,
   point the way they point, and do not touch. What looks like a decision
   made by the school is only ever these, made by each fish. */
#define SCHOOL_RANGE   150.0f    /* how far a fish counts as a neighbour */
#define SCHOOL_APART    20.0f    /* closer than this and it pulls away   */
#define SCHOOL_TOGETHER  0.55f   /* weights, relative to cruising speed  */
#define SCHOOL_MATCH     0.35f
#define SCHOOL_SPACE     1.10f

/* How far a fish notices something that eats fish, how fast the alarm
   builds, and how long it outlives the thing that caused it. A school
   stays scattered for a while after whatever it was has moved off. */
#define FISH_WARY      260.0f
#define FISH_ALARM       2.6f
#define FISH_CALM        0.55f   /* alarm lost per second */

/* How much of the alarm comes from a predator simply being there rather
   than from the noise it makes. Low on purpose: without it a drifting
   cat would be as frightening as a charging one and nothing in the water
   could ever be caught. Not zero, because a shark hanging silently over
   a school is still a shark. */
#define FISH_PRESENCE    0.18f

/* Sharks make their living on these, not on the cat. Faster than a fish
   flat out and slower than the charge it saves for the cat: a chase is
   won, but not instantly, and a school has time to scatter around it
   first. */
#define SHARK_SNAP      34.0f
#define SHARK_HUNT     120.0f

/* And then it stops. Measured without this: a shark that never tires of
   hunting simply lives inside the school, which holds every fish in it
   at full alarm forever - so the school never re-forms, never calms, and
   nothing slower than a shark can ever catch one of them, the cat
   included. A fed shark leaving is what gives the water its quiet. */
#define SHARK_FULL      45.0f

/* Long enough down that coming back up reads as an event, and long
   enough up that it can actually climb the whole column first. */
#define WHALE_SOUND  150.0f
#define WHALE_RISE    60.0f

#define SHARK_SENSE   540.0f
#define SHARK_BITE     46.0f
#define SHARK_DAMAGE    0.25f
#define SHARK_COOL      2.6f

#define SPAWN_NEAR   1500.0f
#define SPAWN_CLEAR   340.0f
#define DESPAWN      2600.0f

typedef struct Aquatic {
    AquaticKind kind;
    Vector2 pos;
    Vector2 vel;
    float   facing;
    float   size;
    float   phase;       /* drives the swim animation      */
    float   timer;
    float   cooldown;
    float   interest;    /* sharks only                     */
    float   home;        /* 0 top of its band, 1 the bottom */
    float   sated;       /* sharks only: time left not hunting */
    bool    rising;      /* whales only: on the way up      */
    bool    active;
} Aquatic;

/* Which entry in the shared table each of these is. The kinds are this
   module's own vocabulary; species.h is everybody else's, and the food
   web is written in that one. */
static const Species AS_SPECIES[AQUA_KIND_COUNT] = {
    [AQUA_JELLY] = SPECIES_JELLY,
    [AQUA_SHARK] = SPECIES_SHARK,
    [AQUA_WHALE] = SPECIES_WHALE,
    [AQUA_FISH]  = SPECIES_FISH,
};

/* How close the cat has to be to take hold of one. A jellyfish does not
   evade and barely swims, so it is caught by swimming into it; nothing
   else down here is picked up at all. */
static float CatchReach(const Aquatic *a)
{
    if (a->kind == AQUA_JELLY) return 26.0f;

    /* A fish that has not noticed is nearly as easy as a jellyfish; one
       that has is most of the way to gone. `interest` is how alarmed it
       is - the same field the shark uses for the opposite feeling. */
    if (a->kind == AQUA_FISH) return (a->interest > 0.4f) ? 8.0f : 24.0f;

    return 0.0f;
}

#define AQUATIC_SEED 0x5EA51DEu

static Aquatic sLife[AQUATIC_MAX];

/* Its own stream. See core/rng.h. */
static Rng sRng;

static float Rand01(void)                  { return Rng01(&sRng); }
static float RandRange(float lo, float hi) { return RngBetween(&sRng, lo, hi); }

/* Eaten, by anything. The pool is shared across kinds, so one handler
   serves all of them. */
static void AquaticEaten(int tag)
{
    if (tag < 0 || tag >= AQUATIC_MAX) return;

    sLife[tag].active = false;
}

void AquaticReset(void)
{
    for (int i = 0; i < AQUATIC_MAX; i++) sLife[i].active = false;

    RngSeed(&sRng, AQUATIC_SEED);

    for (int k = 0; k < AQUA_KIND_COUNT; k++)
    {
        CreaturesOnRemove(AS_SPECIES[k], AquaticEaten);
    }
}

int AquaticCount(void)
{
    int n = 0;
    for (int i = 0; i < AQUATIC_MAX; i++) if (sLife[i].active) n++;
    return n;
}

int AquaticCountOf(AquaticKind kind)
{
    int n = 0;
    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (sLife[i].active && sLife[i].kind == kind) n++;
    }
    return n;
}

bool        AquaticActive(int i)   { return (i >= 0 && i < AQUATIC_MAX) && sLife[i].active; }
AquaticKind AquaticKindOf(int i)   { return AquaticActive(i) ? sLife[i].kind : AQUA_JELLY; }
Vector2     AquaticPosition(int i) { return AquaticActive(i) ? sLife[i].pos : (Vector2){ 0, 0 }; }
float       AquaticAlarm(int i)     { return (AquaticActive(i) && sLife[i].kind == AQUA_FISH)
                                            ? sLife[i].interest : 0.0f; }
Species     AquaticSpeciesOf(int i) { return AS_SPECIES[AquaticKindOf(i)]; }
bool        AquaticHunting(int i)  { return AquaticActive(i) && sLife[i].kind == AQUA_SHARK &&
                                            sLife[i].interest > 0.5f; }

/* How much of itself a thing gives away. A hunting shark is unmissable
   whatever it does, which is why fish scatter from one long before it is
   close; everything else down here is quiet. */
static float NoiseOf(const Aquatic *a)
{
    if (a->kind == AQUA_SHARK) return (a->interest > 0.5f) ? 1.0f : 0.75f;
    if (a->kind == AQUA_WHALE) return 0.60f;
    if (a->kind == AQUA_FISH)  return 0.10f + a->interest * 0.40f;

    return 0.05f;
}

static float SizeOf(AquaticKind kind)
{
    return (kind == AQUA_WHALE) ? 120.0f
         : (kind == AQUA_SHARK) ?  34.0f
         : (kind == AQUA_FISH)  ?  16.0f
                                :  14.0f;
}

/* Deep by day, shallow by night, measured from wherever in its own band
   this individual sits. Kept free of the struct so spawning can ask the
   same question before there is anything to ask it about. */
static float DepthFor(AquaticKind kind, float home, float worldX)
{
    const Band *b = &BANDS[kind];

    float t = home + b->migrate * (DaylightBrightness() - 0.5f);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;

    float want = b->shallow + (b->deep - b->shallow) * t;

    /* Its own size counts against the clearance, or a whale aims at a
       depth its body does not fit in and fails to spawn at all. */
    float bed = OceanFloorAt(worldX) - WeatherWaterY()
              - FLOOR_CLEAR - SizeOf(kind);
    if (want > bed) want = bed;
    if (want < b->shallow) want = b->shallow;

    return want;
}

static float AquaticGlowOf(const Aquatic *a)
{
    if (a->kind != AQUA_JELLY) return 0.0f;

    /* A slow pulse, out of step between individuals. */
    float pulse = 0.55f + 0.45f * sinf(a->phase * 1.6f);

    /* Bioluminescence is only worth anything where there is no daylight
       left to drown it out: what reaches this one is the sky's own
       brightness, minus everything the water above took out of it. So a
       jellyfish at the surface at noon barely shows, and the same
       jellyfish at midnight - or four hundred down at any hour - is the
       only light there is. */
    float reaching = OceanLight(OceanDepthAt(a->pos.y)) * DaylightBrightness();

    return pulse * (0.15f + 0.85f * (1.0f - reaching));
}

float AquaticGlow(int i)
{
    if (!AquaticActive(i)) return 0.0f;

    return AquaticGlowOf(&sLife[i]);
}

/* --- spawning ---------------------------------------------------------- */

/* Open water: under the surface and not inside anything. */
static bool OpenWaterAt(Vector2 p, float size)
{
    if (p.y < WeatherWaterY() + SEA_TOP) return false;

    Rectangle box = { p.x - size, p.y - size, size * 2.0f, size * 2.0f };

    return !TerrainOverlaps(box);
}

static bool Place(AquaticKind kind, float x, float y, float size, float home)
{
    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (sLife[i].active) continue;

        sLife[i].kind = kind;
        sLife[i].pos = (Vector2){ x, y };
        sLife[i].vel = (Vector2){ 0.0f, 0.0f };
        sLife[i].facing = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        sLife[i].size = size;
        sLife[i].phase = Rand01() * 6.28f;
        sLife[i].timer = RandRange(2.0f, 6.0f);
        sLife[i].cooldown = 0.0f;
        sLife[i].interest = 0.0f;
        sLife[i].home = home;
        sLife[i].sated = 0.0f;
        sLife[i].rising = false;
        sLife[i].active = true;
        return true;
    }

    return false;
}

static void TrySpawn(AquaticKind kind, float centreX)
{
    float size = SizeOf(kind);

    for (int attempt = 0; attempt < 10; attempt++)
    {
        float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        float x = centreX + side * RandRange(SPAWN_CLEAR, SPAWN_NEAR);

        /* Spawn where this kind would already be at this hour, not at a
           uniform depth it would then have to swim away from. */
        float home = RandRange(BANDS[kind].homeLo, BANDS[kind].homeHi);
        Vector2 p = { x, WeatherWaterY() + DepthFor(kind, home, x) };

        if (!OpenWaterAt(p, size)) continue;

        Place(kind, p.x, p.y, size, home);
        return;
    }
}

/* Fish arrive as a school rather than one at a time. A lone fish in open
   water is not a thing that happens, and a school assembled out of
   singletons would spend a minute looking like an accident first. */
#define SCHOOL_SIZE 10

static void TrySpawnSchool(float centreX)
{
    float size = SizeOf(AQUA_FISH);

    for (int attempt = 0; attempt < 10; attempt++)
    {
        float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        float x = centreX + side * RandRange(SPAWN_CLEAR, SPAWN_NEAR);

        /* One home depth for the whole school: they are neighbours
           because they want the same water, not by coincidence. */
        float home = RandRange(BANDS[AQUA_FISH].homeLo, BANDS[AQUA_FISH].homeHi);
        Vector2 p = { x, WeatherWaterY() + DepthFor(AQUA_FISH, home, x) };

        /* Room for all of them, not just for one. */
        if (!OpenWaterAt(p, size * 3.0f)) continue;

        int room = FISH_MAX - AquaticCountOf(AQUA_FISH);
        int want = (room < SCHOOL_SIZE) ? room : SCHOOL_SIZE;

        for (int i = 0; i < want; i++)
        {
            Vector2 at = { p.x + RandRange(-70.0f, 70.0f),
                           p.y + RandRange(-40.0f, 40.0f) };

            if (!OpenWaterAt(at, size)) continue;

            Place(AQUA_FISH, at.x, at.y, size, home);
        }

        return;
    }
}

void AquaticForceSpawn(AquaticKind kind, float x)
{
    float size = SizeOf(kind);

    Place(kind, x, WeatherWaterY() + DepthFor(kind, 0.5f, x), size, 0.5f);
}

/* --- behaviour --------------------------------------------------------- */

/* Ease toward the depth this one wants to be at, capped so nothing
   rockets vertically. Returns a velocity, to be added to whatever the
   animal was doing anyway. */
static float DepthDrive(const Aquatic *a, float want, float most)
{
    float v = (want - OceanDepthAt(a->pos.y)) * 0.30f;

    if (v >  most) v =  most;
    if (v < -most) v = -most;

    return v;
}

/* The three schooling rules, measured against every other fish close
   enough to count. Returned as one velocity, because from the fish's
   point of view it was never three decisions.

   Read off this module's own pool rather than the census: they are all
   right here, and this is the one query in the game that wants the
   current tick rather than the last one. */
static Vector2 SchoolPull(const Aquatic *self)
{
    Vector2 centre = { 0.0f, 0.0f };
    Vector2 heading = { 0.0f, 0.0f };
    Vector2 apart = { 0.0f, 0.0f };

    int neighbours = 0;

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        const Aquatic *o = &sLife[i];

        if (o == self || !o->active || o->kind != AQUA_FISH) continue;

        float dx = o->pos.x - self->pos.x;
        float dy = o->pos.y - self->pos.y;
        float d = sqrtf(dx * dx + dy * dy);

        if (d > SCHOOL_RANGE) continue;

        neighbours++;

        centre.x += dx;
        centre.y += dy;

        heading.x += o->vel.x;
        heading.y += o->vel.y;

        /* Crowding pushes hardest at the closest range, which is what
           keeps a school a school instead of a heap. */
        if (d < SCHOOL_APART && d > 0.01f)
        {
            float push = (SCHOOL_APART - d) / SCHOOL_APART;

            apart.x -= dx / d * push;
            apart.y -= dy / d * push;
        }
    }

    if (neighbours == 0) return (Vector2){ 0.0f, 0.0f };

    Vector2 out = { 0.0f, 0.0f };
    float n = (float)neighbours;

    /* Toward the middle of them. */
    float len = sqrtf(centre.x * centre.x + centre.y * centre.y);

    if (len > 1.0f)
    {
        out.x += centre.x / len * FISH_CRUISE * SCHOOL_TOGETHER;
        out.y += centre.y / len * FISH_CRUISE * SCHOOL_TOGETHER;
    }

    /* Going the way they are going. */
    out.x += heading.x / n * SCHOOL_MATCH;
    out.y += heading.y / n * SCHOOL_MATCH;

    /* And not into any of them. Capped, or a fish in the middle of a
       tight knot is thrown further than it can swim. */
    float shove = sqrtf(apart.x * apart.x + apart.y * apart.y);

    if (shove > 1.0f) { apart.x /= shove; apart.y /= shove; }

    out.x += apart.x * FISH_CRUISE * SCHOOL_SPACE;
    out.y += apart.y * FISH_CRUISE * SCHOOL_SPACE;

    return out;
}

static void Swim(Aquatic *a, Vector2 wanted, float accel, float dt)
{
    a->vel.x += (wanted.x - a->vel.x) * accel * dt;
    a->vel.y += (wanted.y - a->vel.y) * accel * dt;

    Vector2 next = { a->pos.x + a->vel.x * dt, a->pos.y + a->vel.y * dt };

    /* Turn at the roof of the sea and at anything solid, rather than
       beaching. Each kind has its own roof: what a jellyfish can slip
       under a whale cannot. */
    float top = WeatherWaterY() + BANDS[a->kind].shallow;

    if (next.y < top) { next.y = top; a->vel.y = fabsf(a->vel.y) * 0.4f; }

    /* Its own band is the limit, not one shared ceiling for everything. */
    if (next.y > WeatherWaterY() + BANDS[a->kind].deep + 150.0f)
    {
        a->vel.y = -fabsf(a->vel.y);
    }

    float halfW = a->size * 0.6f;
    float halfH = a->size * 0.4f;

    /* Each axis on its own, so an obstruction turns it instead of
       pinning it. Testing only the corner it wanted meant a whale
       climbing into the underside of the city stopped dead and stayed
       there for the rest of its rise, going nowhere in either
       direction. */
    Rectangle acrossOnly = { next.x - halfW, a->pos.y - halfH,
                             halfW * 2.0f, halfH * 2.0f };
    Rectangle upOnly     = { a->pos.x - halfW, next.y - halfH,
                             halfW * 2.0f, halfH * 2.0f };

    bool blockedAcross = TerrainOverlaps(acrossOnly);
    bool blockedUp     = TerrainOverlaps(upOnly);

    if (!blockedAcross) a->pos.x = next.x;
    else
    {
        /* Bouncing alone pins anything that wants to be on the other
           side. Measured with a shark chasing a school: it sat at one x
           for nine seconds, turning into the same wall every tick while
           what it was chasing swam out of range. So look for a way over
           or under it, and take whichever is open. A wall in the ruins
           is nearly always something to get around rather than the edge
           of the world. */
        float climb = fabsf(a->vel.x) * 0.8f;

        Rectangle above = { a->pos.x - halfW, a->pos.y - halfH * 3.0f,
                            halfW * 2.0f, halfH * 2.0f };
        Rectangle below = { a->pos.x - halfW, a->pos.y + halfH,
                            halfW * 2.0f, halfH * 2.0f };

        if      (!TerrainOverlaps(above)) a->vel.y -= climb;
        else if (!TerrainOverlaps(below)) a->vel.y += climb;

        a->vel.x = -a->vel.x * 0.6f;
        a->facing = -a->facing;
    }

    if (!blockedUp) a->pos.y = next.y;
    else a->vel.y = -a->vel.y * 0.4f;

    if (fabsf(a->vel.x) > 4.0f) a->facing = (a->vel.x > 0.0f) ? 1.0f : -1.0f;

    a->phase += dt * (1.0f + fabsf(a->vel.x) * 0.02f);
}

static void UpdateOne(Aquatic *a, float dt, Vector2 cat, bool catSwimming)
{
    if (a->cooldown > 0.0f) a->cooldown -= dt;

    float dx = cat.x - a->pos.x;
    float dy = cat.y - a->pos.y;
    float distance = sqrtf(dx * dx + dy * dy);

    switch (a->kind)
    {
        case AQUA_SHARK:
        {
            if (a->sated > 0.0f) a->sated -= dt;

            /* Only interested in something actually in the water. */
            if (catSwimming && distance < SHARK_SENSE)
            {
                a->interest += (1.0f - distance / SHARK_SENSE) * 1.4f * dt;
            }

            a->interest -= 0.30f * dt;
            if (a->interest < 0.0f) a->interest = 0.0f;
            if (a->interest > 1.4f) a->interest = 1.4f;

            Vector2 wanted;

            if (a->interest > 0.5f && distance > 0.1f)
            {
                wanted = (Vector2){ dx / distance * SHARK_CHARGE,
                                    dy / distance * SHARK_CHARGE };
            }
            else
            {
                /* Not after the cat, so it is doing what it does for a
                   living. The census says where the fish are; this code
                   only knows it is hungry. */
                const Creature *quarry = (a->sated > 0.0f) ? NULL
                    : CreaturesNearest(SPECIES_BIT(SPECIES_FISH),
                                       a->pos, SHARK_SENSE);

                if (quarry)
                {
                    float qx = quarry->pos.x - a->pos.x;
                    float qy = quarry->pos.y - a->pos.y;
                    float qd = sqrtf(qx * qx + qy * qy);

                    if (qd > 0.1f)
                    {
                        wanted = (Vector2){ qx / qd * SHARK_HUNT,
                                            qy / qd * SHARK_HUNT };
                    }
                    else wanted = (Vector2){ 0.0f, 0.0f };

                    /* And takes one. A shark thinning out a school is
                       the only place in the game the food chain is
                       visible with the cat nowhere in it. */
                    if (qd < SHARK_SNAP && a->cooldown <= 0.0f &&
                        CreaturesConsume(quarry))
                    {
                        a->cooldown = 1.4f;
                        a->sated = RandRange(SHARK_FULL * 0.7f,
                                             SHARK_FULL * 1.3f);
                    }
                }
                else
                {
                    a->timer -= dt;
                    if (a->timer <= 0.0f)
                    {
                        a->facing = -a->facing;
                        a->timer = RandRange(3.0f, 8.0f);
                    }

                    /* Only when it is not chasing anything: a shark with
                       something in sight goes where that is, not where
                       the day says it should be. */
                    float drift = DepthDrive(a, DepthFor(a->kind, a->home, a->pos.x),
                                             SHARK_CRUISE * 0.35f);

                    wanted = (Vector2){ a->facing * SHARK_CRUISE,
                                        drift + sinf(a->phase * 0.6f) * 18.0f };
                }
            }

            Swim(a, wanted, 3.0f, dt);

            if (distance < SHARK_BITE && a->cooldown <= 0.0f && catSwimming)
            {
                a->cooldown = SHARK_COOL;
                VitalsApply(0.0f, -SHARK_DAMAGE, 0.0f);
                CatShove((dx > 0.0f ? 1.0f : -1.0f) * 220.0f, -120.0f);
            }
            break;
        }

        case AQUA_WHALE:
        {
            /* Straight on, slowly, indifferent to the cat. It works the
               whole column: down to the plain to feed, then all the way
               back up until the drowned city stops it. Nothing else has
               a reason to cross every zone, and below the shelf it is
               the only thing that goes deeper than the cat can follow. */
            a->timer -= dt;

            if (a->timer <= 0.0f)
            {
                a->rising = !a->rising;
                a->timer = a->rising
                         ? WHALE_RISE
                         : RandRange(WHALE_SOUND * 0.7f, WHALE_SOUND * 1.3f);
            }

            float want = a->rising ? BANDS[AQUA_WHALE].shallow
                                   : DepthFor(a->kind, a->home, a->pos.x);

            Vector2 wanted = { a->facing * WHALE_SPEED,
                               DepthDrive(a, want, WHALE_SPEED * 2.0f)
                                   + sinf(a->phase * 0.25f) * 8.0f };
            Swim(a, wanted, 0.6f, dt);
            break;
        }

        case AQUA_FISH:
        {
            /* Anything that eats fish, and is actually down here to do
               it - the census does not know the cat has climbed out.
               Which species those are is species.h's business; this one
               has never heard of a shark. */
            const Creature *danger =
                CreaturesNearestThreat(SPECIES_FISH, a->pos, FISH_WARY);

            if (danger && danger->pos.y < WeatherWaterY()) danger = NULL;

            /* Alarm outlives the thing that caused it, so a school stays
               scattered for a while after whatever it was moves off. */
            a->interest -= FISH_CALM * dt;

            Vector2 wanted = SchoolPull(a);

            if (danger)
            {
                float ax = a->pos.x - danger->pos.x;
                float ay = a->pos.y - danger->pos.y;
                float d = sqrtf(ax * ax + ay * ay);

                /* Alarm builds the way a rat's does, out of how close it
                   is and how much noise it is making - not out of being
                   seen. A shark is loud by nature and they scatter from
                   one at range; a cat that drifts in is very nearly
                   nothing, and that gap is the only way it ever eats. */
                float closeness = 1.0f - d / FISH_WARY;

                a->interest += closeness
                             * (FISH_PRESENCE + (1.0f - FISH_PRESENCE) * danger->noise)
                             * FISH_ALARM * dt;

                /* Bolting is proportional to the alarm, so a calm fish
                   does not quietly edge away from something it has not
                   noticed. */
                if (d > 0.1f)
                {
                    wanted.x += ax / d * FISH_DART * a->interest;
                    wanted.y += ay / d * FISH_DART * a->interest;
                }
            }

            if (a->interest < 0.0f) a->interest = 0.0f;
            if (a->interest > 1.0f) a->interest = 1.0f;

            /* Its own band, underneath all of that. */
            wanted.y += DepthDrive(a, DepthFor(a->kind, a->home, a->pos.x),
                                   FISH_CRUISE * 0.6f);

            /* With nobody near and nothing to run from it still goes
               somewhere, or a lone fish would hang in the water. */
            a->timer -= dt;

            if (a->timer <= 0.0f)
            {
                a->facing = -a->facing;
                a->timer = RandRange(3.0f, 9.0f);
            }

            wanted.x += a->facing * FISH_CRUISE * 0.5f;

            /* A frightened one stops schooling politely and just goes.
               Flat out it is faster than the cat swims, which is what
               makes catching one a matter of getting there first. */
            float most = FISH_CRUISE + (FISH_DART - FISH_CRUISE) * a->interest;
            float len = sqrtf(wanted.x * wanted.x + wanted.y * wanted.y);

            if (len > most)
            {
                wanted.x = wanted.x / len * most;
                wanted.y = wanted.y / len * most;
            }

            Swim(a, wanted, 4.0f, dt);
            break;
        }

        case AQUA_JELLY:
        default:
        {
            /* Pulse upward, then sink. Nothing else: the cat can swim
               straight through one, which is what makes the light worth
               swimming towards. */
            float pulse = sinf(a->phase * 1.6f);

            /* The pulse and the sink stay exactly as they were - the
               migration is a bias underneath them, not a replacement.
               Up close a jellyfish still bobs; it is only over an hour
               that you notice the whole layer has moved. */
            float drift = DepthDrive(a, DepthFor(a->kind, a->home, a->pos.x),
                                     JELLY_SPEED * 0.5f);

            Vector2 wanted = { a->facing * JELLY_SPEED * 0.4f,
                               drift + ((pulse > 0.6f) ? -JELLY_SPEED : 12.0f) };

            Swim(a, wanted, 1.6f, dt);

            a->timer -= dt;
            if (a->timer <= 0.0f)
            {
                a->facing = -a->facing;
                a->timer = RandRange(4.0f, 10.0f);
            }

            break;
        }
    }
}

void AquaticFixedUpdate(float dt)
{
    Vector2 cat = CatPosition();
    bool swimming = CatIsSwimming();

    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        if (!sLife[i].active) continue;

        if (fabsf(sLife[i].pos.x - cat.x) > DESPAWN)
        {
            sLife[i].active = false;
            continue;
        }

        /* The flood can drop away underneath them. */
        if (sLife[i].pos.y < WeatherWaterY() + SEA_TOP * 0.5f)
        {
            sLife[i].active = false;
            continue;
        }

        UpdateOne(&sLife[i], dt, cat, swimming);

        CreaturesPublish(AS_SPECIES[sLife[i].kind], sLife[i].pos, i,
                         CatchReach(&sLife[i]), NoiseOf(&sLife[i]));
    }

    if (Rand01() < 0.02f)
    {
        if (AquaticCountOf(AQUA_JELLY) < JELLY_MAX) TrySpawn(AQUA_JELLY, cat.x);
    }

    if (Rand01() < 0.004f)
    {
        if (AquaticCountOf(AQUA_SHARK) < SHARK_MAX) TrySpawn(AQUA_SHARK, cat.x);
    }

    if (Rand01() < 0.0015f)
    {
        if (AquaticCountOf(AQUA_WHALE) < WHALE_MAX) TrySpawn(AQUA_WHALE, cat.x);
    }

    /* Topped up only once it is well down, so schools arrive as schools
       and get eaten down to nothing before another one turns up. */
    if (Rand01() < 0.010f)
    {
        if (AquaticCountOf(AQUA_FISH) < FISH_MAX / 2) TrySpawnSchool(cat.x);
    }
}

/* --- drawing -----------------------------------------------------------
   Drawn as shapes rather than bitmaps: a whale is ten times a jellyfish,
   and one grid would not serve both. */

static void DrawJelly(const Aquatic *a)
{
    float s = a->size;
    float pulse = 0.75f + 0.25f * sinf(a->phase * 1.6f);
    float glow = AquaticGlowOf(a);

    Color bell = (Color){ 150, 120, 210, 255 };
    Color rim  = (Color){ 206, 190, 245, 255 };

    /* Bell: a squat dome that squashes as it pulses. */
    float w = s * 2.0f * (2.0f - pulse) * 0.55f;
    float h = s * pulse;

    for (float row = 0.0f; row < h; row += 2.0f)
    {
        float t = row / h;
        float half = (w * 0.5f) * sqrtf(1.0f - t * t);

        DrawRectangleRec((Rectangle){ a->pos.x - half, a->pos.y - h + row, half * 2.0f, 2.0f },
                         Fade(bell, 0.55f + glow * 0.30f));
    }

    DrawRectangleRec((Rectangle){ a->pos.x - w * 0.5f, a->pos.y - 2.0f, w, 2.0f },
                     Fade(rim, 0.6f + glow * 0.4f));

    /* Tentacles, trailing and out of phase with each other. */
    for (int i = 0; i < 5; i++)
    {
        float ox = (-2.0f + (float)i) * (w * 0.16f);

        for (float d = 0.0f; d < s * 2.4f; d += 3.0f)
        {
            float sway = sinf(a->phase * 2.0f + d * 0.08f + (float)i) * (d * 0.10f);

            DrawRectangleRec((Rectangle){ a->pos.x + ox + sway, a->pos.y + d, 2.0f, 3.0f },
                             Fade(rim, (1.0f - d / (s * 2.4f)) * 0.45f));
        }
    }
}

static void DrawShark(const Aquatic *a)
{
    float s = a->size;
    float f = a->facing;

    Color body = (Color){ 44, 52, 62, 255 };
    Color belly = (Color){ 78, 88, 98, 255 };
    Color eye = (Color){ 210, 90, 70, 255 };

    /* Body: long, tapering both ways. */
    for (float t = -1.0f; t < 1.0f; t += 0.04f)
    {
        float half = (1.0f - t * t) * s * 0.40f;
        if (half < 1.0f) continue;

        float x = a->pos.x + t * s * f;

        DrawRectangleRec((Rectangle){ x, a->pos.y - half, 3.0f, half * 2.0f }, body);
        DrawRectangleRec((Rectangle){ x, a->pos.y + half * 0.35f, 3.0f, half * 0.65f }, belly);
    }

    /* Dorsal fin. */
    for (float d = 0.0f; d < s * 0.35f; d += 2.0f)
    {
        float wide = (1.0f - d / (s * 0.35f)) * s * 0.30f;

        DrawRectangleRec((Rectangle){ a->pos.x - wide * 0.5f - s * 0.05f * f,
                                      a->pos.y - s * 0.36f - d, wide, 2.0f }, body);
    }

    /* Tail, sweeping. */
    float sweep = sinf(a->phase * 3.0f) * s * 0.22f;
    float tx = a->pos.x - s * f;

    for (float d = 0.0f; d < s * 0.45f; d += 2.0f)
    {
        float spread = d * 0.55f;

        DrawRectangleRec((Rectangle){ tx - d * f * 0.5f,
                                      a->pos.y - spread + sweep * (d / (s * 0.45f)),
                                      3.0f, spread * 2.0f }, body);
    }

    DrawRectangleRec((Rectangle){ a->pos.x + s * 0.55f * f, a->pos.y - s * 0.10f, 3.0f, 3.0f },
                     eye);
}

/* Small, and there are two dozen of them, so this stays cheap. The back
   is dark and the flank is not: a school changing direction flickers,
   which is most of what a school looks like from any distance. */
static void DrawFish(const Aquatic *a)
{
    float s = a->size;
    float f = a->facing;

    Color back  = (Color){  52,  70,  84, 255 };
    Color flank = (Color){ 118, 142, 156, 255 };

    for (float t = -1.0f; t < 1.0f; t += 0.10f)
    {
        float half = (1.0f - t * t) * s * 0.34f;
        if (half < 0.8f) continue;

        float x = a->pos.x + t * s * 0.5f * f;

        DrawRectangleRec((Rectangle){ x, a->pos.y - half, 2.0f, half * 2.0f }, back);
        DrawRectangleRec((Rectangle){ x, a->pos.y - half * 0.1f, 2.0f, half }, flank);
    }

    /* Tail. The phase already runs faster the harder it is swimming, so
       a bolting fish beats visibly quicker than a drifting one. */
    float sweep = sinf(a->phase * 5.0f) * s * 0.16f;
    float tx = a->pos.x - s * 0.5f * f;

    for (float d = 0.0f; d < s * 0.26f; d += 2.0f)
    {
        float spread = d * 0.7f;

        DrawRectangleRec((Rectangle){ tx - d * f * 0.6f,
                                      a->pos.y - spread + sweep * (d / (s * 0.26f)),
                                      2.0f, spread * 2.0f }, back);
    }
}

static void DrawWhale(const Aquatic *a)
{
    float s = a->size;
    float f = a->facing;

    Color body = (Color){ 30, 36, 46, 255 };
    Color belly = (Color){ 52, 60, 72, 255 };

    for (float t = -1.0f; t < 1.0f; t += 0.02f)
    {
        float taper = (t < -0.6f) ? (1.0f - (-0.6f - t) / 0.4f) : 1.0f;
        float half = (1.0f - t * t * 0.55f) * s * 0.30f * taper;
        if (half < 1.0f) continue;

        float x = a->pos.x + t * s * f;

        DrawRectangleRec((Rectangle){ x, a->pos.y - half, 4.0f, half * 2.0f }, body);
        DrawRectangleRec((Rectangle){ x, a->pos.y + half * 0.5f, 4.0f, half * 0.5f }, belly);
    }

    /* Fluke. */
    float sweep = sinf(a->phase * 0.8f) * s * 0.16f;
    float tx = a->pos.x - s * 1.02f * f;

    for (float d = 0.0f; d < s * 0.30f; d += 3.0f)
    {
        float spread = d * 0.9f;

        DrawRectangleRec((Rectangle){ tx - d * f * 0.4f,
                                      a->pos.y - spread + sweep, 4.0f, spread * 2.0f }, body);
    }
}

void AquaticDraw(float alpha, float left, float right)
{
    (void)alpha;

    /* Whales first, so smaller things swim in front of them. */
    for (int pass = 0; pass < 2; pass++)
    {
        for (int i = 0; i < AQUATIC_MAX; i++)
        {
            if (!sLife[i].active) continue;

            bool big = (sLife[i].kind == AQUA_WHALE);
            if ((pass == 0) != big) continue;

            float reach = sLife[i].size * 2.5f;
            if (sLife[i].pos.x + reach < left || sLife[i].pos.x - reach > right) continue;

            switch (sLife[i].kind)
            {
                case AQUA_SHARK: DrawShark(&sLife[i]); break;
                case AQUA_WHALE: DrawWhale(&sLife[i]); break;
                case AQUA_FISH:  DrawFish(&sLife[i]);  break;
                default:         DrawJelly(&sLife[i]); break;
            }
        }
    }
}
