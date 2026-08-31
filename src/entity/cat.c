#include "entity/cat.h"
#include "entity/cat_art.h"
#include "core/input.h"
#include "entity/vitals.h"
#include "world/physics.h"
#include "world/terrain.h"
#include "world/weather.h"

#include <math.h>

/* World units are design pixels, +y down. */
#define BODY_W        22.0f
#define BODY_H        30.0f
#define BODY_H_CROUCH 20.0f

#define GRAVITY     1900.0f
#define MAX_FALL    1300.0f

#define SPEED_WALK    96.0f
#define SPEED_RUN    200.0f
#define SPEED_CROUCH   46.0f

#define ACCEL_GROUND 1300.0f
#define ACCEL_AIR     720.0f
#define FRICTION     1700.0f

#define JUMP_VELOCITY -600.0f
#define COYOTE_TIME     0.10f
#define JUMP_BUFFER     0.12f

/* Bob cycles per unit travelled. At 0.10 a sprint bobbed twenty times a
   second, which reads as vibration rather than running. A gait is two to
   four steps a second. */
#define STRIDE_RATE     0.018f

/* Water: cats can swim, but they are slow, loud and hate it. */
#define SWIM_ACCEL    460.0f
#define SWIM_MAX       74.0f
#define SWIM_SINK     240.0f
#define SWIM_BUOYANCY 900.0f
#define SWIM_DRAG       2.4f
/* Buoyancy settles the cat at SWIM_SINK / (SWIM_BUOYANCY * 0.02) below
   the surface, which is 13.3. The kick window has to be deeper than that
   or it can never fire and the water is a trap - which it was. */
#define SWIM_DIVE     520.0f    /* pushing down against buoyancy */
#define SWIM_RISE     460.0f
#define SWIM_VMAX     150.0f

#define SWIM_SURFACE   28.0f
#define SWIM_KICK       0.88f

typedef struct Cat {
    Body     body;
    CatState state;
    float    facing;       /* +1 right, -1 left */
    bool     crouching;
    bool     swimming;
    bool     submerged;
    float    coyote;
    float    buffer;
    float    stride;       /* walk-cycle phase */
    float    noise;
    float    stillFor;     /* seconds spent doing nothing */
    float    blinkWait;    /* seconds until the next blink */
    float    blinkFor;     /* seconds of blink remaining */
    unsigned int rng;      /* local, so blinking cannot perturb worldgen */
} Cat;

static Cat sCat;

/* --- sprite ------------------------------------------------------------
   Art lives in cat_art.c; this file only decides how it is posed. World
   units per art pixel: 21 * 1.5 = 31.5, matching the collision box. */

#define CAT_CELL 1.5f

/* How long the cat must stand still before it sits down. Sitting is the
   pose the reference art was drawn in, so idling earns it. */
#define SIT_AFTER 3.5f

/* Local LCG: using the global one would advance the sequence world
   generation depends on. */
static float CatRandom(void)
{
    sCat.rng = sCat.rng * 1664525u + 1013904223u;
    return (float)((sCat.rng >> 8) & 0xFFFFu) / 65535.0f;
}

static float BodyHeight(void)
{
    return sCat.crouching ? BODY_H_CROUCH : BODY_H;
}

Rectangle CatBounds(void)
{
    return BodyRect(&sCat.body);
}

void CatSpawn(Vector2 position)
{
    BodyInit(&sCat.body, position, BODY_W, BODY_H);

    sCat.state = CAT_IDLE;
    sCat.facing = 1.0f;
    sCat.crouching = false;
    sCat.swimming = false;
    sCat.coyote = 0.0f;
    sCat.buffer = 0.0f;
    sCat.stride = 0.0f;
    sCat.noise = 0.0f;
    sCat.stillFor = 0.0f;
    sCat.blinkWait = 2.0f;
    sCat.blinkFor = 0.0f;
    sCat.rng = 0x9E3779B9u;
}

static void UpdateNoise(void)
{
    float base = 0.0f;
    float speed = fabsf(sCat.body.vel.x);

    switch (sCat.state)
    {
        case CAT_IDLE:   base = 0.02f; break;
        case CAT_CROUCH: base = 0.06f + speed / SPEED_CROUCH * 0.04f; break;
        case CAT_WALK:   base = 0.30f; break;
        case CAT_RUN:    base = 0.78f; break;
        case CAT_AIR:    base = 0.20f; break;
        case CAT_SWIM:   base = 0.62f; break;
        default: break;
    }

    /* Rain covers a great deal, but never all of it. */
    sCat.noise = base * (1.0f - WeatherNoiseMask());
}

void CatFixedUpdate(float dt)
{
    BodyBeginTick(&sCat.body);

    float move = InputAxisX();
    if (move != 0.0f) sCat.facing = (move > 0.0f) ? 1.0f : -1.0f;

    /* Submerged once the body's midpoint is under the surface. */
    float waterY = WeatherWaterY();
    float midY = sCat.body.pos.y - BodyHeight() * 0.5f;
    sCat.swimming = (midY > waterY);

    /* The head, not the middle: you can swim along the top and breathe.
       Air trapped under rock counts as a surface, which is the only way
       anything gets below the shelf and back. */
    Vector2 head = { sCat.body.pos.x, sCat.body.pos.y - BodyHeight() };

    sCat.submerged = (head.y > waterY) && !TerrainAirAt(head);

    if (InputPressed(ACT_JUMP)) sCat.buffer = JUMP_BUFFER;
    if (sCat.buffer > 0.0f) sCat.buffer -= dt;
    if (sCat.coyote > 0.0f) sCat.coyote -= dt;

    if (sCat.swimming)
    {
        sCat.crouching = false;

        sCat.body.vel.x += move * SWIM_ACCEL * dt;
        sCat.body.vel.x -= sCat.body.vel.x * SWIM_DRAG * dt;

        if (sCat.body.vel.x >  SWIM_MAX) sCat.body.vel.x =  SWIM_MAX;
        if (sCat.body.vel.x < -SWIM_MAX) sCat.body.vel.x = -SWIM_MAX;

        float depth = midY - waterY;

        /* Holding down beats buoyancy; that is what diving is. */
        bool diving = InputDown(ACT_DOWN) || InputDown(ACT_CROUCH);
        bool rising = InputDown(ACT_UP);

        if (diving)       sCat.body.vel.y += SWIM_DIVE * dt;
        else if (rising)  sCat.body.vel.y -= SWIM_RISE * dt;
        else              sCat.body.vel.y += (SWIM_SINK - depth * SWIM_BUOYANCY * 0.02f) * dt;

        sCat.body.vel.y -= sCat.body.vel.y * SWIM_DRAG * dt;

        if (sCat.body.vel.y >  SWIM_VMAX) sCat.body.vel.y =  SWIM_VMAX;
        if (sCat.body.vel.y < -SWIM_VMAX) sCat.body.vel.y = -SWIM_VMAX;

        /* Kicking off at the surface, to get back onto a ledge. */
        if (sCat.buffer > 0.0f && !diving && depth < SWIM_SURFACE)
        {
            sCat.body.vel.y = JUMP_VELOCITY * SWIM_KICK;
            sCat.buffer = 0.0f;
        }

        sCat.state = CAT_SWIM;
    }
    else
    {
        bool wantCrouch = InputDown(ACT_CROUCH) && sCat.body.grounded;

        /* Never stand up into a ceiling. */
        if (sCat.crouching && !wantCrouch)
        {
            if (!TerrainOverlaps(BodyRectAt(&sCat.body, sCat.body.pos, BODY_H))) sCat.crouching = false;
        }
        else
        {
            sCat.crouching = wantCrouch;
        }

        /* Out of breath means you simply cannot sprint. */
        bool canRun = InputDown(ACT_RUN) && VitalsHasStamina(0.02f);
        float top = sCat.crouching ? SPEED_CROUCH : (canRun ? SPEED_RUN : SPEED_WALK);
        float accel = sCat.body.grounded ? ACCEL_GROUND : ACCEL_AIR;

        if (move != 0.0f)
        {
            sCat.body.vel.x += move * accel * dt;
            if (sCat.body.vel.x >  top) sCat.body.vel.x =  top;
            if (sCat.body.vel.x < -top) sCat.body.vel.x = -top;
        }
        else if (sCat.body.grounded)
        {
            float drop = FRICTION * dt;
            if (fabsf(sCat.body.vel.x) <= drop) sCat.body.vel.x = 0.0f;
            else sCat.body.vel.x -= drop * (sCat.body.vel.x > 0.0f ? 1.0f : -1.0f);
        }

        if (sCat.body.grounded) sCat.coyote = COYOTE_TIME;

        if (sCat.buffer > 0.0f && sCat.coyote > 0.0f)
        {
            sCat.body.vel.y = JUMP_VELOCITY * (sCat.crouching ? 0.75f : 1.0f);
            VitalsSpendStamina(0.06f);
            sCat.buffer = 0.0f;
            sCat.coyote = 0.0f;
            sCat.body.grounded = false;
        }

        /* Releasing jump early cuts the arc short. */
        if (!InputDown(ACT_JUMP) && sCat.body.vel.y < 0.0f) sCat.body.vel.y += GRAVITY * 1.6f * dt;

        BodyApplyGravity(&sCat.body, GRAVITY, MAX_FALL, dt);
    }

    /* Crouching changes the collision height, so keep the body in sync
       before it moves. */
    sCat.body.height = BodyHeight();
    BodyMove(&sCat.body, dt);

    if (!sCat.swimming)
    {
        if (!sCat.body.grounded)             sCat.state = CAT_AIR;
        else if (sCat.crouching)        sCat.state = CAT_CROUCH;
        else if (fabsf(sCat.body.vel.x) < 4.0f) sCat.state = CAT_IDLE;
        else if (fabsf(sCat.body.vel.x) > SPEED_WALK + 6.0f) sCat.state = CAT_RUN;
        else                            sCat.state = CAT_WALK;
    }

    /* Stride drives the bob; it only advances when the cat does. */
    sCat.stride += fabsf(sCat.body.vel.x) * dt * STRIDE_RATE;

    if (sCat.state == CAT_IDLE) sCat.stillFor += dt;
    else                        sCat.stillFor = 0.0f;

    /* Blink on an explicit timer. Every 2.4-6.4s, held long enough to
       actually land on a frame. */
    if (sCat.blinkFor > 0.0f) sCat.blinkFor -= dt;

    sCat.blinkWait -= dt;
    if (sCat.blinkWait <= 0.0f)
    {
        sCat.blinkFor = 0.12f;
        sCat.blinkWait = 2.4f + CatRandom() * 4.0f;
    }

    UpdateNoise();
}

Vector2  CatPosition(void)     { return sCat.body.pos; }

Vector2 CatRenderPosition(float alpha)
{
    return BodyRenderPos(&sCat.body, alpha);
}

CatState CatCurrentState(void) { return sCat.state; }
bool     CatIsSwimming(void)   { return sCat.swimming; }
bool     CatIsSubmerged(void)  { return sCat.submerged; }
float    CatNoise(void)        { return sCat.noise; }
float    CatVelocityX(void)    { return sCat.body.vel.x; }

void CatShove(float vx, float vy)
{
    sCat.body.vel.x = vx;
    sCat.body.vel.y = vy;
    sCat.body.grounded = false;
}

/* Exposed so the tests can check the tuning rather than restate it. */
float CatStrideRate(void)   { return STRIDE_RATE; }
float CatRunSpeed(void)     { return SPEED_RUN; }

float CatSwimRestDepth(void)
{
    return SWIM_SINK / (SWIM_BUOYANCY * 0.02f);
}

float CatSwimKickWindow(void) { return SWIM_SURFACE; }

float CatMaxJumpHeight(void)
{
    /* v^2 / 2g, with the jump held for its full arc. */
    return (JUMP_VELOCITY * JUMP_VELOCITY) / (2.0f * GRAVITY);
}

float CatMaxRunJumpDistance(void)
{
    return (2.0f * -JUMP_VELOCITY / GRAVITY) * SPEED_RUN;
}

void CatDraw(float alpha)
{
    Vector2 at = CatRenderPosition(alpha);

    float cellW = CAT_CELL;
    float cellH = CAT_CELL;
    float bob = 0.0f;

    switch (sCat.state)
    {
        case CAT_WALK:
        case CAT_RUN:
            bob = -fabsf(sinf(sCat.stride * 6.28f)) * CAT_CELL * 0.9f;
            break;

        case CAT_CROUCH:
            cellH = CAT_CELL * 0.72f;
            cellW = CAT_CELL * 1.08f;
            break;

        case CAT_AIR:
            cellH = CAT_CELL * 1.08f;
            cellW = CAT_CELL * 0.94f;
            break;

        case CAT_SWIM:
            bob = sinf((float)GetTime() * 3.0f) * CAT_CELL * 0.4f;
            break;

        default: break;
    }

    bool mirrored = (sCat.facing * CAT_ART_AUTHORED_FACING) < 0.0f;
    float bodyCol = mirrored ? ((float)CAT_ART_W - 1.0f - CAT_ART_BODY_COL)
                             : CAT_ART_BODY_COL;

    float eyes = (sCat.blinkFor > 0.0f) ? 0.0f : 1.0f;
    /* Clip at the waterline only while floating on it. Once the cat is
       fully under, clipping hid the whole animal - the water is drawn over
       it afterwards anyway, which is what tints it. */
    float clip = (sCat.swimming && !sCat.submerged) ? WeatherWaterY() : 0.0f;

    CatArtDrawFrame(CatArtSit, CAT_ART_W, CAT_ART_H,
                    at.x - bodyCol * cellW,
                    at.y - (float)CAT_ART_H * cellH + bob,
                    cellW, cellH, sCat.facing, eyes, 1.0f, clip);

    /* Bubbles, so being under reads as being under. */
    if (sCat.submerged)
    {
        float t = (float)GetTime();

        for (int i = 0; i < 3; i++)
        {
            float phase = fmodf(t * 0.8f + (float)i * 0.37f, 1.0f);
            float bx = at.x + sinf(t * 2.0f + (float)i * 2.1f) * 5.0f
                            + sCat.facing * -8.0f;
            float by = at.y - BodyHeight() - phase * 34.0f;

            float side = (1.0f - phase * 0.6f) * 3.0f;

            DrawRectangleRec((Rectangle){ bx, by, side, side },
                             Fade((Color){ 196, 226, 236, 255 }, (1.0f - phase) * 0.55f));
        }
    }
}
