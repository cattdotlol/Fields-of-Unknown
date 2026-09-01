#include "entity/rat.h"
#include "core/rng.h"
#include "entity/cat.h"
#include "gfx/sprite.h"
#include "world/physics.h"
#include "world/terrain.h"

#include <math.h>

#define BODY_W  16.0f
#define BODY_H  10.0f

#define GRAVITY   1900.0f
#define MAX_FALL  1300.0f

#define SPEED_FORAGE  38.0f
#define SPEED_SNIFF    0.0f
#define SPEED_SCURRY  86.0f     /* short bursts while foraging */
#define SPEED_WARY    82.0f
#define SPEED_FLEE   150.0f     /* slower than a sprinting cat, on purpose */

/* Rats accelerate. Setting velocity directly is what made them read as
   sliding blocks rather than animals. */
#define ACCEL_NORMAL  480.0f
#define ACCEL_PANIC   900.0f

#define HOP_VELOCITY (-340.0f)

/* Prey freezes before it runs. This is the gap between "heard something"
   and "gone", and it is the window the cat is playing for. */
#define ALERT_FREEZE   0.32f
#define FREEZE_MAX     2.4f

/* One bolting rat takes the others with it. */
#define PANIC_SPREAD 230.0f
#define PANIC_RISE     1.3f

/* How far a rat keeps an eye out, and how much noise it takes to spook
   one at that range. */
#define HEAR_RANGE   460.0f
#define ALERT_RISE     2.6f
#define ALERT_FALL     0.55f
#define ALERT_PANIC    0.70f

/* Close enough to smell. A rat that is almost underfoot should bolt
   whatever the cat is doing - the previous value lost the race against
   its own alert decay, so a rat could be stood on for nearly two seconds
   without reacting. */
#define TOUCH_RANGE    90.0f
#define TOUCH_RISE      1.6f

#define KEEP_NEAR   1600.0f     /* rats live within this of the cat */
#define DESPAWN     2600.0f
#define SPAWN_CLEAR  420.0f     /* never appear in the cat's lap */

#define CATCH_UNAWARE  30.0f
#define CATCH_FLEEING  15.0f

typedef struct Rat {
    Body     body;
    RatState state;
    float    facing;
    float    timer;        /* time left in the current bit of behaviour */
    float    alert;
    float    stride;
    bool     active;

    /* Individual variation, so a group does not move as one organism. */
    float    nervous;      /* how fast alarm builds                     */
    float    speedScale;
    float    restless;     /* how long it will settle for               */

    float    burst;        /* scurry burst remaining                    */
    float    dart;         /* committed to running past the cat         */
    float    wary;         /* time left being jumpy after a scare       */
    float    blocked;      /* how long it has been walking into a wall  */
} Rat;

#define RAT_SEED 0xC0FFEEu

static Rat sRats[RAT_MAX];

/* Its own stream, so rats deciding which way to turn cannot perturb
   world generation. See core/rng.h. */
static Rng sRng;

static float Rand01(void)                  { return Rng01(&sRng); }
static float RandRange(float lo, float hi) { return RngBetween(&sRng, lo, hi); }

void RatsReset(void)
{
    for (int i = 0; i < RAT_MAX; i++) sRats[i].active = false;

    RngSeed(&sRng, RAT_SEED);
}

int RatCount(void)
{
    int n = 0;
    for (int i = 0; i < RAT_MAX; i++) if (sRats[i].active) n++;
    return n;
}

int RatAlarmed(void)
{
    int n = 0;
    for (int i = 0; i < RAT_MAX; i++)
    {
        if (sRats[i].active && sRats[i].state == RAT_FLEE) n++;
    }
    return n;
}

bool RatActive(int index)
{
    if (index < 0 || index >= RAT_MAX) return false;
    return sRats[index].active;
}

Vector2 RatPosition(int index)
{
    if (index < 0 || index >= RAT_MAX) return (Vector2){ 0.0f, 0.0f };
    return sRats[index].body.pos;
}

RatState RatCurrentState(int index)
{
    if (index < 0 || index >= RAT_MAX) return RAT_FORAGE;
    return sRats[index].state;
}

float RatAlertLevel(int index)
{
    if (index < 0 || index >= RAT_MAX) return 0.0f;
    return sRats[index].alert;
}

float RatVelocityX(int index)
{
    if (index < 0 || index >= RAT_MAX) return 0.0f;
    return sRats[index].body.vel.x;
}

/* --- spawning ---------------------------------------------------------- */

/* A rat is narrow, so it needs very little of a ledge to stand on. */
#define FOOTING 10.0f

/* Spawns near a given x rather than near the cat. */
static void TrySpawnAt(float centreX)
{
    for (int attempt = 0; attempt < 12; attempt++)
    {
        float x = centreX + RandRange(-160.0f, 160.0f);
        float ground = TerrainDryGroundAt(x, FOOTING);
        if (ground < 0.0f) continue;

        for (int i = 0; i < RAT_MAX; i++)
        {
            if (sRats[i].active) continue;

            BodyInit(&sRats[i].body, (Vector2){ x, ground }, BODY_W, BODY_H);
            sRats[i].state = RAT_FORAGE;
            sRats[i].facing = (Rand01() < 0.5f) ? -1.0f : 1.0f;
            sRats[i].timer = RandRange(1.0f, 3.0f);
            sRats[i].alert = 0.0f;
            sRats[i].stride = 0.0f;
            sRats[i].nervous = RandRange(0.70f, 1.50f);
            sRats[i].speedScale = RandRange(0.85f, 1.20f);
            sRats[i].restless = RandRange(0.70f, 1.40f);
            sRats[i].burst = 0.0f;
            sRats[i].dart = 0.0f;
            sRats[i].wary = 0.0f;
            sRats[i].blocked = 0.0f;
            sRats[i].active = true;
            return;
        }
        return;
    }
}

static void TrySpawn(float catX)
{
    for (int attempt = 0; attempt < 8; attempt++)
    {
        float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        float x = catX + side * RandRange(SPAWN_CLEAR, KEEP_NEAR);

        float ground = TerrainDryGroundAt(x, FOOTING);
        if (ground < 0.0f) continue;

        for (int i = 0; i < RAT_MAX; i++)
        {
            if (sRats[i].active) continue;

            BodyInit(&sRats[i].body, (Vector2){ x, ground }, BODY_W, BODY_H);

            sRats[i].state = RAT_FORAGE;
            sRats[i].facing = (Rand01() < 0.5f) ? -1.0f : 1.0f;
            sRats[i].timer = RandRange(1.0f, 3.0f);
            sRats[i].alert = 0.0f;
            sRats[i].stride = 0.0f;

            sRats[i].nervous = RandRange(0.70f, 1.50f);
            sRats[i].speedScale = RandRange(0.85f, 1.20f);
            sRats[i].restless = RandRange(0.70f, 1.40f);

            sRats[i].burst = 0.0f;
            sRats[i].dart = 0.0f;
            sRats[i].wary = 0.0f;
            sRats[i].blocked = 0.0f;

            sRats[i].active = true;
            return;
        }

        return;     /* pool full */
    }
}

void RatsForceSpawn(float x)
{
    TrySpawnAt(x);
}

/* --- behaviour --------------------------------------------------------- */

/* How far ahead it checks for floor and for walls. A rat looks barely
   past its own nose, which is why one can be cornered against a channel
   at all. */
#define LOOK_DOWN  6.0f
#define LOOK_WALL  4.0f

static void UpdateOne(Rat *r, float dt, Vector2 catPos, float catNoise)
{
    BodyBeginTick(&r->body);

    float dx = catPos.x - r->body.pos.x;
    float dy = catPos.y - r->body.pos.y;
    float distance = sqrtf(dx * dx + dy * dy);
    float towardCat = (dx > 0.0f) ? 1.0f : -1.0f;

    /* --- hearing ------------------------------------------------------
       A nervous rat startles at half what a bold one ignores. */
    if (distance < HEAR_RANGE)
    {
        float closeness = 1.0f - (distance / HEAR_RANGE);
        r->alert += catNoise * closeness * ALERT_RISE * r->nervous * dt;

        if (distance < TOUCH_RANGE) r->alert += TOUCH_RISE * r->nervous * dt;
    }

    /* Being recently scared keeps them on edge for a while. */
    float settle = (r->wary > 0.0f) ? ALERT_FALL * 0.45f : ALERT_FALL;
    r->alert -= settle * dt;

    if (r->alert < 0.0f) r->alert = 0.0f;
    if (r->alert > 1.6f) r->alert = 1.6f;

    if (r->wary > 0.0f) r->wary -= dt;
    if (r->dart > 0.0f) r->dart -= dt;
    if (r->burst > 0.0f) r->burst -= dt;

    /* --- transitions --------------------------------------------------- */
    if (r->alert >= ALERT_PANIC)
    {
        r->state = RAT_FLEE;
    }
    else if (r->state == RAT_FORAGE && r->alert >= ALERT_FREEZE)
    {
        /* Stop dead and listen. This is the tell the player reads. */
        r->state = RAT_FREEZE;
        r->timer = FREEZE_MAX;
    }
    else if (r->state == RAT_FREEZE)
    {
        r->timer -= dt;

        /* Nerve gives out before the cat does. */
        if (r->timer <= 0.0f) r->state = RAT_FLEE;
        else if (r->alert < ALERT_FREEZE * 0.5f) r->state = RAT_FORAGE;
    }
    else if (r->state == RAT_FLEE && r->alert < 0.25f)
    {
        r->state = RAT_WARY;
        r->wary = RandRange(4.0f, 9.0f);
        r->timer = r->wary;
    }
    else if (r->state == RAT_WARY && r->wary <= 0.0f)
    {
        r->state = RAT_FORAGE;
        r->timer = RandRange(1.0f, 3.0f) * r->restless;
    }

    /* --- what that means for movement ---------------------------------- */
    float wanted = 0.0f;
    float accel = ACCEL_NORMAL;

    switch (r->state)
    {
        case RAT_FLEE:
        {
            accel = ACCEL_PANIC;

            /* Committed to slipping past the cat, do not re-think it. */
            if (r->dart <= 0.0f) r->facing = -towardCat;

            wanted = r->facing * SPEED_FLEE * r->speedScale;
            break;
        }

        case RAT_WARY:
            /* Keeps its distance without full panic. */
            if (distance < 260.0f) r->facing = -towardCat;
            wanted = r->facing * SPEED_WARY * r->speedScale;
            break;

        case RAT_FREEZE:
            wanted = SPEED_SNIFF;
            accel = ACCEL_PANIC;      /* stopping is fast */
            break;

        case RAT_FORAGE:
        default:
            r->timer -= dt;

            if (r->timer <= 0.0f)
            {
                float roll = Rand01();

                if (roll < 0.35f)
                {
                    /* Nose down, going nowhere. */
                    r->timer = RandRange(0.7f, 2.2f) * r->restless;
                    r->burst = 0.0f;
                    r->facing = r->facing;
                    r->stride = r->stride;
                    r->timer = RandRange(0.7f, 2.2f) * r->restless;
                    r->burst = -1.0f;          /* marks a sniff */
                }
                else if (roll < 0.60f)
                {
                    r->facing = -r->facing;
                    r->timer = RandRange(1.2f, 3.0f) * r->restless;
                    r->burst = 0.0f;
                }
                else
                {
                    /* A short scurry, the way they actually move. */
                    r->timer = RandRange(1.5f, 3.5f) * r->restless;
                    r->burst = RandRange(0.35f, 0.9f);
                }
            }

            if (r->burst < 0.0f)      wanted = 0.0f;
            else if (r->burst > 0.0f) wanted = r->facing * SPEED_SCURRY * r->speedScale;
            else                      wanted = r->facing * SPEED_FORAGE * r->speedScale;
            break;
    }

    /* --- the world gets a say ------------------------------------------ */
    bool moving = (wanted != 0.0f);

    if (moving && r->body.grounded && !BodyGroundAhead(&r->body, r->facing, LOOK_DOWN, 10.0f))
    {
        if (r->state == RAT_FLEE && r->dart <= 0.0f && distance < 150.0f)
        {
            /* Cornered against a channel with the cat closing: bolt past
               it rather than jittering on the lip. */
            r->facing = towardCat;
            r->dart = 0.9f;
            wanted = r->facing * SPEED_FLEE * r->speedScale;
        }
        else
        {
            /* Otherwise stop at the edge. Rats do not drown themselves. */
            r->facing = -r->facing;
            wanted = 0.0f;
        }
    }

    /* Walking into a wall: hop it, if it is hoppable. */
    if (moving && BodyWallAhead(&r->body, r->facing, LOOK_WALL, 6.0f))
    {
        r->blocked += dt;

        if (r->body.grounded && r->blocked > 0.15f)
        {
            r->body.vel.y = HOP_VELOCITY;
            r->blocked = 0.0f;
        }
    }
    else
    {
        r->blocked = 0.0f;
    }

    BodySteerX(&r->body, wanted, accel, dt);

    BodyApplyGravity(&r->body, GRAVITY, MAX_FALL, dt);
    BodyMove(&r->body, dt);

    /* Same fix as the cat: a fleeing rat was flickering at 13Hz. */
    r->stride += fabsf(r->body.vel.x) * dt * 0.030f;
}

void RatsFixedUpdate(float dt)
{
    Vector2 catPos = CatPosition();
    float catNoise = CatNoise();

    int alive = 0;

    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!sRats[i].active) continue;

        /* Out of mind once far enough away; the pool refills near the cat. */
        if (fabsf(sRats[i].body.pos.x - catPos.x) > DESPAWN)
        {
            sRats[i].active = false;
            continue;
        }

        UpdateOne(&sRats[i], dt, catPos, catNoise);
        alive++;
    }

    /* One rat bolting takes the others with it - a group scatters, it
       does not stand around watching one of them run. */
    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!sRats[i].active || sRats[i].state != RAT_FLEE) continue;

        for (int j = 0; j < RAT_MAX; j++)
        {
            if (j == i || !sRats[j].active || sRats[j].state == RAT_FLEE) continue;

            float gap = fabsf(sRats[j].body.pos.x - sRats[i].body.pos.x);
            if (gap > PANIC_SPREAD) continue;

            float closeness = 1.0f - (gap / PANIC_SPREAD);
            sRats[j].alert += PANIC_RISE * closeness * sRats[j].nervous * dt;
        }
    }

    /* Trickle in rather than popping a crowd into existence at once. */
    if (alive < RAT_TARGET && Rand01() < 0.03f) TrySpawn(catPos.x);
}

/* --- catching ---------------------------------------------------------- */

int RatCatchable(Rectangle catBox)
{
    float cx = catBox.x + catBox.width * 0.5f;
    float cy = catBox.y + catBox.height * 0.5f;

    int best = -1;
    float bestDistance = 0.0f;

    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!sRats[i].active) continue;

        float dx = sRats[i].body.pos.x - cx;
        float dy = (sRats[i].body.pos.y - BODY_H * 0.5f) - cy;
        float distance = sqrtf(dx * dx + dy * dy);

        float reach = (sRats[i].state == RAT_FLEE) ? CATCH_FLEEING : CATCH_UNAWARE;

        if (distance > reach) continue;
        if (best >= 0 && distance >= bestDistance) continue;

        best = i;
        bestDistance = distance;
    }

    return best;
}

void RatConsume(int index)
{
    if (index < 0 || index >= RAT_MAX) return;

    sRats[index].active = false;
}

/* --- drawing ----------------------------------------------------------- */

#define SPR_W 16
#define SPR_H 8
#define SPR_PIXEL 1.5f

/* Authored facing LEFT, like everything else. F fur, K outline, E eye,
   T tail. */
static const char *const SPRITE[SPR_H] = {
    "...KK...........",
    "..KFFK..........",
    ".KFFFFFFFFFFK...",
    "KEFFFFFFFFFFFKT.",
    ".KFFFFFFFFFFK.TT",
    "..KFFFFFFFFK....",
    "...K.K...K.K....",
    "...K.K...K.K....",
};

static const Sprite RAT_ART = { SPRITE, SPR_W, SPR_H, -1.0f };

static Color RatColor(char cell, const void *ctx)
{
    (void)ctx;

    switch (cell)
    {
        case 'K': return (Color){  26,  22,  22, 255 };   /* outline */
        case 'E': return (Color){ 190,  80,  70, 255 };   /* eye     */
        case 'T': return (Color){ 120, 104,  98, 255 };   /* tail    */
        case 'F': return (Color){  62,  54,  50, 255 };   /* fur     */
        default:  return BLANK;
    }
}

void RatsDraw(float alpha, float left, float right)
{
    float margin = SpriteHalfWidth(RAT_ART, SPR_PIXEL) + 24.0f;

    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!sRats[i].active) continue;

        Vector2 at = BodyRenderPos(&sRats[i].body, alpha);
        if (at.x < left - margin || at.x > right + margin) continue;

        /* Bob follows actual speed rather than the state, so a rat
           easing to a stop settles instead of snapping still. */
        float speed = fabsf(sRats[i].body.vel.x);
        float gait = (speed < 4.0f) ? 0.0f : (speed / SPEED_FLEE);
        if (gait > 1.0f) gait = 1.0f;

        at.y -= fabsf(sinf(sRats[i].stride * 6.28f)) * (0.8f + gait * 1.6f);

        SpriteDrawStanding(RAT_ART, RatColor, NULL, at, SPR_PIXEL,
                           sRats[i].facing, 1.0f);
    }
}
