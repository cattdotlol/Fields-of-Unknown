#include "entity/aquatic.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "world/daylight.h"
#include "world/ocean.h"
#include "world/terrain.h"
#include "world/weather.h"

#include <math.h>

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
    [AQUA_SHARK] = { SEA_TOP,         620.0f, 0.55f, 0.25f, 0.75f },
    /* A whale sits at the bottom of its own range rather than the middle
       of it, and needs more room under the city than the others. Its
       floor is past the cat's crush depth, so following one all the way
       down is not an option. */
    [AQUA_WHALE] = { SEA_TOP + 40.0f, 1800.0f, 0.00f, 0.75f, 1.00f },
};

#define JELLY_MAX  8
#define SHARK_MAX  2
#define WHALE_MAX  1

#define JELLY_SPEED   26.0f
#define SHARK_CRUISE  70.0f
#define SHARK_CHARGE 205.0f      /* the cat swims at 74 */
#define WHALE_SPEED   34.0f

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
    bool    rising;      /* whales only: on the way up      */
    bool    active;
} Aquatic;

static Aquatic sLife[AQUATIC_MAX];
static unsigned int sRng = 0x5EA51DEu;

static float Rand01(void)
{
    sRng = sRng * 1664525u + 1013904223u;
    return (float)((sRng >> 8) & 0xFFFFu) / 65535.0f;
}

static float RandRange(float lo, float hi)
{
    return lo + Rand01() * (hi - lo);
}

void AquaticReset(void)
{
    for (int i = 0; i < AQUATIC_MAX; i++) sLife[i].active = false;

    sRng = 0x5EA51DEu;
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
bool        AquaticHunting(int i)  { return AquaticActive(i) && sLife[i].kind == AQUA_SHARK &&
                                            sLife[i].interest > 0.5f; }

static float SizeOf(AquaticKind kind)
{
    return (kind == AQUA_WHALE) ? 120.0f
         : (kind == AQUA_SHARK) ?  34.0f
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
                a->timer -= dt;
                if (a->timer <= 0.0f)
                {
                    a->facing = -a->facing;
                    a->timer = RandRange(3.0f, 8.0f);
                }

                /* Only when it is not chasing: a shark that has the cat
                   in sight goes where the cat is, not where the day
                   says it should be. */
                float drift = DepthDrive(a, DepthFor(a->kind, a->home, a->pos.x),
                                         SHARK_CRUISE * 0.35f);

                wanted = (Vector2){ a->facing * SHARK_CRUISE,
                                    drift + sinf(a->phase * 0.6f) * 18.0f };
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
                default:         DrawJelly(&sLife[i]); break;
            }
        }
    }
}
