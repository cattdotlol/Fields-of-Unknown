#include "entity/aquatic.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "world/terrain.h"
#include "world/weather.h"

#include <math.h>

/* How far below the surface things live. Nothing swims in the air. */
#define BAND_TOP     26.0f
#define BAND_BOTTOM 360.0f

#define JELLY_MAX  8
#define SHARK_MAX  2
#define WHALE_MAX  1

#define JELLY_SPEED   26.0f
#define SHARK_CRUISE  70.0f
#define SHARK_CHARGE 205.0f      /* the cat swims at 74 */
#define WHALE_SPEED   34.0f

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

static float AquaticGlowOf(const Aquatic *a)
{
    if (a->kind != AQUA_JELLY) return 0.0f;

    /* A slow pulse, out of step between individuals. */
    return 0.55f + 0.45f * sinf(a->phase * 1.6f);
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
    if (p.y < WeatherWaterY() + BAND_TOP) return false;

    Rectangle box = { p.x - size, p.y - size, size * 2.0f, size * 2.0f };

    return !TerrainOverlaps(box);
}

static bool Place(AquaticKind kind, float x, float y, float size)
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
        sLife[i].active = true;
        return true;
    }

    return false;
}

static void TrySpawn(AquaticKind kind, float centreX)
{
    float size = (kind == AQUA_WHALE) ? 120.0f
               : (kind == AQUA_SHARK) ? 34.0f : 14.0f;

    for (int attempt = 0; attempt < 10; attempt++)
    {
        float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        float x = centreX + side * RandRange(SPAWN_CLEAR, SPAWN_NEAR);

        /* Whales want depth; jellyfish are happy near the top. */
        float depth = (kind == AQUA_WHALE) ? RandRange(180.0f, BAND_BOTTOM)
                                           : RandRange(BAND_TOP + 20.0f, BAND_BOTTOM * 0.7f);

        Vector2 p = { x, WeatherWaterY() + depth };

        if (!OpenWaterAt(p, size)) continue;

        Place(kind, p.x, p.y, size);
        return;
    }
}

void AquaticForceSpawn(AquaticKind kind, float x)
{
    float size = (kind == AQUA_WHALE) ? 120.0f
               : (kind == AQUA_SHARK) ? 34.0f : 14.0f;

    float depth = (kind == AQUA_WHALE) ? 220.0f : 80.0f;

    Place(kind, x, WeatherWaterY() + depth, size);
}

/* --- behaviour --------------------------------------------------------- */

static void Swim(Aquatic *a, Vector2 wanted, float accel, float dt)
{
    a->vel.x += (wanted.x - a->vel.x) * accel * dt;
    a->vel.y += (wanted.y - a->vel.y) * accel * dt;

    Vector2 next = { a->pos.x + a->vel.x * dt, a->pos.y + a->vel.y * dt };

    /* Turn at the surface and at anything solid, rather than beaching. */
    float top = WeatherWaterY() + BAND_TOP;

    if (next.y < top) { next.y = top; a->vel.y = fabsf(a->vel.y) * 0.4f; }
    if (next.y > WeatherWaterY() + BAND_BOTTOM) a->vel.y = -fabsf(a->vel.y);

    Rectangle probe = { next.x - a->size * 0.6f, next.y - a->size * 0.4f,
                        a->size * 1.2f, a->size * 0.8f };

    if (TerrainOverlaps(probe))
    {
        a->vel.x = -a->vel.x * 0.6f;
        a->facing = -a->facing;
    }
    else
    {
        a->pos = next;
    }

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

                wanted = (Vector2){ a->facing * SHARK_CRUISE,
                                    sinf(a->phase * 0.6f) * 18.0f };
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
            /* Straight on, slowly, indifferent to everything. */
            Vector2 wanted = { a->facing * WHALE_SPEED, sinf(a->phase * 0.25f) * 8.0f };
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
            Vector2 wanted = { a->facing * JELLY_SPEED * 0.4f,
                               (pulse > 0.6f) ? -JELLY_SPEED : 12.0f };

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
        if (sLife[i].pos.y < WeatherWaterY() + BAND_TOP * 0.5f)
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
