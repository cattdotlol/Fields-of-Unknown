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

#define SPEED_WANDER  42.0f
#define SPEED_FLEE   150.0f     /* slower than a sprinting cat, on purpose */

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
    float    timer;
    float    alert;
    float    stride;
    bool     active;
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
    if (index < 0 || index >= RAT_MAX) return RAT_WANDER;
    return sRats[index].state;
}

float RatAlertLevel(int index)
{
    if (index < 0 || index >= RAT_MAX) return 0.0f;
    return sRats[index].alert;
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

            sRats[i].state = RAT_WANDER;
            sRats[i].facing = (Rand01() < 0.5f) ? -1.0f : 1.0f;
            sRats[i].timer = RandRange(1.0f, 3.0f);
            sRats[i].alert = 0.0f;
            sRats[i].stride = 0.0f;
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

static void UpdateOne(Rat *r, float dt, Vector2 catPos, float catNoise)
{
    BodyBeginTick(&r->body);

    float dx = catPos.x - r->body.pos.x;
    float dy = catPos.y - r->body.pos.y;
    float distance = sqrtf(dx * dx + dy * dy);

    /* Hearing, not sight: a crouching cat can walk right up, a sprinting
       one clears the room. */
    if (distance < HEAR_RANGE)
    {
        float closeness = 1.0f - (distance / HEAR_RANGE);
        r->alert += catNoise * closeness * ALERT_RISE * dt;

        /* Something almost on top of them registers regardless. */
        if (distance < TOUCH_RANGE) r->alert += TOUCH_RISE * dt;
    }

    r->alert -= ALERT_FALL * dt;

    if (r->alert < 0.0f) r->alert = 0.0f;
    if (r->alert > 1.5f) r->alert = 1.5f;

    if (r->alert >= ALERT_PANIC) r->state = RAT_FLEE;
    else if (r->state == RAT_FLEE && r->alert < 0.25f) r->state = RAT_WANDER;

    float speed = 0.0f;

    switch (r->state)
    {
        case RAT_FLEE:
            r->facing = (dx > 0.0f) ? -1.0f : 1.0f;      /* directly away */
            speed = SPEED_FLEE;
            break;

        case RAT_PAUSE:
            r->timer -= dt;
            if (r->timer <= 0.0f)
            {
                r->state = RAT_WANDER;
                r->timer = RandRange(1.5f, 4.0f);
            }
            break;

        case RAT_WANDER:
        default:
            speed = SPEED_WANDER;
            r->timer -= dt;

            if (r->timer <= 0.0f)
            {
                /* Stop and sniff, or turn around. */
                if (Rand01() < 0.45f)
                {
                    r->state = RAT_PAUSE;
                    r->timer = RandRange(0.6f, 2.0f);
                }
                else
                {
                    r->facing = -r->facing;
                    r->timer = RandRange(1.5f, 4.0f);
                }
            }
            break;
    }

    /* Turn at a drop rather than walking into the channel. Fleeing rats
       will still stop at the edge - they are cornered, not suicidal. */
    if (speed > 0.0f && r->body.grounded && !GroundAhead(r))
    {
        r->facing = -r->facing;
        if (r->state == RAT_FLEE) speed *= 0.35f;
    }

    r->body.vel.x = r->facing * speed;

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

        /* Scurrying bobs; a paused rat sits still. */
        float bob = (sRats[i].state == RAT_PAUSE) ? 0.0f
                  : -fabsf(sinf(sRats[i].stride * 6.28f)) * 1.5f;

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
