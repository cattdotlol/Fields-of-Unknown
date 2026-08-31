#include "entity/rat.h"
#include "entity/cat.h"
#include "world/physics.h"
#include "world/terrain.h"
#include "world/weather.h"

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

static Rat sRats[RAT_MAX];
static unsigned int sRng = 0xC0FFEEu;

/* Local, so spawning rats cannot perturb world generation. */
static float Rand01(void)
{
    sRng = sRng * 1664525u + 1013904223u;
    return (float)((sRng >> 8) & 0xFFFFu) / 65535.0f;
}

static float RandRange(float lo, float hi)
{
    return lo + Rand01() * (hi - lo);
}

void RatsReset(void)
{
    for (int i = 0; i < RAT_MAX; i++) sRats[i].active = false;

    sRng = 0xC0FFEEu;
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

/* Top of a solid at this x that is out of the water, or -1. */
static float DryGroundAt(float x)
{
    float best = -1.0f;
    float water = WeatherWaterY();

    for (int i = 0; i < TerrainCount(); i++)
    {
        Rectangle r = TerrainSolid(i);

        if (r.height < 40.0f) continue;                 /* not a ledge */
        if (x < r.x + 10.0f || x > r.x + r.width - 10.0f) continue;
        if (r.y >= water) continue;                     /* submerged */

        if (best < 0.0f || r.y < best) best = r.y;
    }

    return best;
}

static void TrySpawn(float catX)
{
    for (int attempt = 0; attempt < 8; attempt++)
    {
        float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        float x = catX + side * RandRange(SPAWN_CLEAR, KEEP_NEAR);

        float ground = DryGroundAt(x);
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

/* --- behaviour --------------------------------------------------------- */

/* Rats will not walk off into the water. Probe just ahead and below. */
static bool GroundAhead(const Rat *r)
{
    float probeX = r->body.pos.x + r->facing * (BODY_W * 0.6f + 6.0f);

    Rectangle foot = { probeX - 3.0f, r->body.pos.y + 2.0f, 6.0f, 10.0f };

    return TerrainOverlaps(foot);
}

/* Something solid at head height in front: a wall it could hop onto
   rather than a drop it should avoid. */
static bool WallAhead(const Rat *r)
{
    float probeX = r->body.pos.x + r->facing * (BODY_W * 0.6f + 4.0f);

    Rectangle chest = { probeX - 3.0f, r->body.pos.y - BODY_H, 6.0f, BODY_H * 0.8f };

    return TerrainOverlaps(chest);
}

/* Ease toward a wanted speed instead of snapping to it. */
static void Steer(Rat *r, float wanted, float accel, float dt)
{
    float diff = wanted - r->body.vel.x;
    float step = accel * dt;

    if (diff >  step) diff =  step;
    if (diff < -step) diff = -step;

    r->body.vel.x += diff;
}

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

    if (moving && r->body.grounded && !GroundAhead(r))
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
    if (moving && WallAhead(r))
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

    Steer(r, wanted, accel, dt);

    BodyApplyGravity(&r->body, GRAVITY, MAX_FALL, dt);
    BodyMove(&r->body, dt);

    r->stride += fabsf(r->body.vel.x) * dt * 0.09f;
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

/* Authored facing LEFT, like everything else. F fur, K outline, E eye,
   T tail. */
static const char *SPRITE[SPR_H] = {
    "...KK...........",
    "..KFFK..........",
    ".KFFFFFFFFFFK...",
    "KEFFFFFFFFFFFKT.",
    ".KFFFFFFFFFFK.TT",
    "..KFFFFFFFFK....",
    "...K.K...K.K....",
    "...K.K...K.K....",
};

void RatsDraw(float alpha, float left, float right)
{
    Color fur  = (Color){ 62, 54, 50, 255 };
    Color dark = (Color){ 26, 22, 22, 255 };
    Color eye  = (Color){ 190, 80, 70, 255 };
    Color tail = (Color){ 120, 104, 98, 255 };

    for (int i = 0; i < RAT_MAX; i++)
    {
        if (!sRats[i].active) continue;

        Vector2 at = BodyRenderPos(&sRats[i].body, alpha);
        if (at.x < left - 60.0f || at.x > right + 60.0f) continue;

        /* Bob follows actual speed rather than the state, so a rat
           easing to a stop settles instead of snapping still. */
        float speed = fabsf(sRats[i].body.vel.x);
        float gait = (speed < 4.0f) ? 0.0f : (speed / SPEED_FLEE);
        if (gait > 1.0f) gait = 1.0f;

        float bob = -fabsf(sinf(sRats[i].stride * 6.28f)) * (0.8f + gait * 1.6f);

        float px = 1.5f;
        float originX = at.x - (float)SPR_W * px * 0.5f;
        float originY = at.y - (float)SPR_H * px + bob;

        for (int row = 0; row < SPR_H; row++)
        {
            for (int col = 0; col < SPR_W; col++)
            {
                int read = (sRats[i].facing > 0.0f) ? (SPR_W - 1 - col) : col;
                char c = SPRITE[row][read];

                if (c == '.') continue;

                Color use = fur;
                if (c == 'K') use = dark;
                else if (c == 'E') use = eye;
                else if (c == 'T') use = tail;

                DrawRectangleRec((Rectangle){ originX + (float)col * px,
                                              originY + (float)row * px, px, px }, use);
            }
        }
    }
}
