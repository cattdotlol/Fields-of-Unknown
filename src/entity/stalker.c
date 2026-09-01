#include "entity/stalker.h"

#include "core/rng.h"
#include "entity/creatures.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "gfx/sprite.h"
#include "world/daylight.h"
#include "world/physics.h"
#include "world/terrain.h"

#include <math.h>

#define BODY_W  40.0f
#define BODY_H  28.0f

#define GRAVITY   1900.0f
#define MAX_FALL  1300.0f

/* Slower than a sprinting cat (200), faster than a walking one (96). You
   can outrun it - but running is exactly what it is listening for. */
#define SPEED_PROWL   58.0f
#define SPEED_HUNT   172.0f
#define SPEED_LUNGE  310.0f
#define ACCEL        620.0f
#define ACCEL_LUNGE 1400.0f

#define HEAR_RANGE   900.0f
#define INTEREST_GAIN  1.8f
#define INTEREST_FALL  0.16f     /* slow: it stays curious for a while */
#define INTEREST_HUNT  0.45f

#define ARRIVE        60.0f      /* close enough to the remembered spot */
#define SEARCH_TIME    6.0f

#define STRIKE_RANGE  52.0f
#define STRIKE_TIME    0.38f
#define STRIKE_DAMAGE  0.20f     /* one heart */
#define STRIKE_COOL    2.2f
#define WITHDRAW_TIME  1.1f

#define SPAWN_MIN    900.0f
#define SPAWN_MAX   1700.0f
#define DESPAWN     3200.0f
#define GRACE       25.0f        /* seconds before the first one appears */

typedef struct Stalker {
    Body         body;
    StalkerState state;
    float        facing;
    float        interest;
    Vector2      heard;       /* last place the cat made a noise */
    float        timer;
    float        cooldown;
    float        speedScale;
    bool         struck;      /* this lunge has already landed */
    bool         active;
} Stalker;

#define STALKER_SEED 0xBADCA7u

static Stalker sPack[STALKER_MAX];
static float sElapsed;
static bool  sRoarPending;
static float sRoarLoud;

/* Its own stream. See core/rng.h. */
static Rng sRng;

static float Rand01(void)                  { return Rng01(&sRng); }
static float RandRange(float lo, float hi) { return RngBetween(&sRng, lo, hi); }

void StalkersReset(void)
{
    for (int i = 0; i < STALKER_MAX; i++) sPack[i].active = false;

    RngSeed(&sRng, STALKER_SEED);
    sElapsed = 0.0f;
    sRoarPending = false;
    sRoarLoud = 0.0f;
}

int StalkerCount(void)
{
    int n = 0;
    for (int i = 0; i < STALKER_MAX; i++) if (sPack[i].active) n++;
    return n;
}

int StalkerHunting(void)
{
    int n = 0;
    for (int i = 0; i < STALKER_MAX; i++)
    {
        if (!sPack[i].active) continue;
        if (sPack[i].state == STALK_HUNT || sPack[i].state == STALK_STRIKE) n++;
    }
    return n;
}

bool StalkerActive(int index)
{
    return (index >= 0 && index < STALKER_MAX) && sPack[index].active;
}

Vector2 StalkerPosition(int index)
{
    if (!StalkerActive(index)) return (Vector2){ 0.0f, 0.0f };
    return sPack[index].body.pos;
}

StalkerState StalkerCurrentState(int index)
{
    if (!StalkerActive(index)) return STALK_PROWL;
    return sPack[index].state;
}

float StalkerInterest(int index)
{
    if (!StalkerActive(index)) return 0.0f;
    return sPack[index].interest;
}

bool StalkerConsumeRoar(float *loudness)
{
    if (!sRoarPending) return false;

    sRoarPending = false;
    if (loudness) *loudness = sRoarLoud;

    return true;
}

float StalkerNearestDistance(void)
{
    float best = -1.0f;
    Vector2 cat = CatPosition();

    for (int i = 0; i < STALKER_MAX; i++)
    {
        if (!sPack[i].active) continue;

        float d = fabsf(sPack[i].body.pos.x - cat.x);
        if (best < 0.0f || d < best) best = d;
    }

    return best;
}

/* --- spawning ---------------------------------------------------------- */

/* Wide enough that it needs a real slab under it, not the lip of one. */
#define FOOTING 24.0f

static void TrySpawn(float catX)
{
    for (int attempt = 0; attempt < 6; attempt++)
    {
        float side = (Rand01() < 0.5f) ? -1.0f : 1.0f;
        float x = catX + side * RandRange(SPAWN_MIN, SPAWN_MAX);

        float ground = TerrainDryGroundAt(x, FOOTING);
        if (ground < 0.0f) continue;

        for (int i = 0; i < STALKER_MAX; i++)
        {
            if (sPack[i].active) continue;

            BodyInit(&sPack[i].body, (Vector2){ x, ground }, BODY_W, BODY_H);

            sPack[i].state = STALK_PROWL;
            sPack[i].facing = -side;      /* facing roughly toward the cat */
            sPack[i].interest = 0.0f;
            sPack[i].heard = (Vector2){ x, ground };
            sPack[i].timer = 0.0f;
            sPack[i].cooldown = 0.0f;
            sPack[i].speedScale = RandRange(0.92f, 1.08f);
            sPack[i].struck = false;
            sPack[i].active = true;
            return;
        }

        return;
    }
}

void StalkersForceSpawn(float x)
{
    sElapsed = GRACE + 1.0f;    /* skip the grace window */
    TrySpawn(x);
}

/* --- behaviour --------------------------------------------------------- */

/* It looks further ahead than a rat does - part of why it never walks
   into the water by mistake. */
#define LOOK_DOWN 8.0f

/* It owns the dark. In daylight it keeps its distance and gives up
   quickly; after sundown it hears further and stays interested far
   longer - and a full moon is genuinely safer than a new one. */
static float Boldness(void)
{
    float b = 1.0f - DaylightBrightness();
    return (b < 0.0f) ? 0.0f : b;
}

static void UpdateOne(Stalker *s, float dt, Vector2 catPos, float catNoise)
{
    BodyBeginTick(&s->body);

    float dx = catPos.x - s->body.pos.x;
    float dy = catPos.y - s->body.pos.y;
    float distance = sqrtf(dx * dx + dy * dy);
    float toward = (dx > 0.0f) ? 1.0f : -1.0f;

    if (s->cooldown > 0.0f) s->cooldown -= dt;

    /* --- hearing: the cat's own noise is what draws it -------------- */
    float bold = Boldness();
    float hearing = HEAR_RANGE * (1.0f + bold * 0.60f);

    if (distance < hearing && catNoise > 0.01f)
    {
        float closeness = 1.0f - (distance / hearing);
        float gain = catNoise * closeness * INTEREST_GAIN
                     * (1.0f + bold * 0.55f) * dt;

        if (gain > 0.0f)
        {
            s->interest += gain;
            s->heard = catPos;          /* it remembers where, not who */
        }
    }

    s->interest -= INTEREST_FALL * (1.0f - bold * 0.55f) * dt;

    if (s->interest < 0.0f) s->interest = 0.0f;
    if (s->interest > 1.4f) s->interest = 1.4f;

    /* --- transitions ------------------------------------------------- */
    switch (s->state)
    {
        case STALK_STRIKE:
            s->timer -= dt;
            if (s->timer <= 0.0f)
            {
                s->state = s->struck ? STALK_WITHDRAW : STALK_HUNT;
                s->timer = WITHDRAW_TIME;
            }
            break;

        case STALK_WITHDRAW:
            s->timer -= dt;
            if (s->timer <= 0.0f) s->state = STALK_HUNT;
            break;

        case STALK_SEARCH:
            s->timer -= dt;
            if (s->interest >= INTEREST_HUNT && fabsf(s->heard.x - s->body.pos.x) > ARRIVE)
            {
                s->state = STALK_HUNT;
            }
            else if (s->timer <= 0.0f)
            {
                s->state = STALK_PROWL;
                s->interest *= 0.4f;
            }
            break;

        case STALK_HUNT:
            if (fabsf(s->heard.x - s->body.pos.x) < ARRIVE)
            {
                s->state = STALK_SEARCH;
                s->timer = SEARCH_TIME;
            }
            else if (s->interest < 0.12f)
            {
                s->state = STALK_PROWL;
            }
            break;

        case STALK_PROWL:
        default:
            if (s->interest >= INTEREST_HUNT)
            {
                s->state = STALK_HUNT;

                /* Announce it. Being hunted silently is not a mechanic,
                   it is an ambush. */
                sRoarPending = true;
                sRoarLoud = 1.0f - (distance / HEAR_RANGE);
                if (sRoarLoud < 0.15f) sRoarLoud = 0.15f;
            }
            break;
    }

    /* Close enough to try, and not still recovering from the last one. */
    if (distance < STRIKE_RANGE && s->cooldown <= 0.0f &&
        s->state != STALK_STRIKE && s->state != STALK_WITHDRAW)
    {
        s->state = STALK_STRIKE;
        s->timer = STRIKE_TIME;
        s->cooldown = STRIKE_COOL;
        s->struck = false;
        s->facing = toward;
    }

    /* --- movement ----------------------------------------------------- */
    float wanted = 0.0f;
    float accel = ACCEL;

    switch (s->state)
    {
        case STALK_STRIKE:
            accel = ACCEL_LUNGE;
            wanted = s->facing * SPEED_LUNGE * s->speedScale;

            /* One hit per lunge. */
            if (!s->struck && distance < BODY_W * 0.7f)
            {
                s->struck = true;
                VitalsApply(0.0f, -STRIKE_DAMAGE, 0.0f);
                CatShove(s->facing * 260.0f, -150.0f);
            }
            break;

        case STALK_WITHDRAW:
            wanted = -s->facing * SPEED_PROWL * 0.8f;
            break;

        case STALK_HUNT:
            s->facing = (s->heard.x > s->body.pos.x) ? 1.0f : -1.0f;
            wanted = s->facing * SPEED_HUNT * s->speedScale;
            break;

        case STALK_SEARCH:
            /* Casting back and forth over the spot. */
            if (fmodf(s->timer, 2.0f) > 1.0f) s->facing = 1.0f;
            else                              s->facing = -1.0f;
            wanted = s->facing * SPEED_PROWL * s->speedScale;
            break;

        case STALK_PROWL:
        default:
            wanted = s->facing * SPEED_PROWL * s->speedScale;
            break;
    }

    /* It will not swim. Water is an escape route, and that is the point. */
    if (wanted != 0.0f && s->body.grounded && !BodyGroundAhead(&s->body, s->facing, LOOK_DOWN, 14.0f))
    {
        s->facing = -s->facing;
        wanted = 0.0f;
    }

    BodySteerX(&s->body, wanted, accel, dt);

    BodyApplyGravity(&s->body, GRAVITY, MAX_FALL, dt);
    BodyMove(&s->body, dt);
}

void StalkersFixedUpdate(float dt)
{
    sElapsed += dt;

    Vector2 catPos = CatPosition();
    float catNoise = CatNoise();

    int alive = 0;

    for (int i = 0; i < STALKER_MAX; i++)
    {
        if (!sPack[i].active) continue;

        if (fabsf(sPack[i].body.pos.x - catPos.x) > DESPAWN)
        {
            sPack[i].active = false;
            continue;
        }

        UpdateOne(&sPack[i], dt, catPos, catNoise);
        alive++;

        /* Nothing eats it, so it is published purely to be feared: a rat
           reads the census for what would eat a rat, and finds this. */
        CreaturesPublish(SPECIES_STALKER,
                         (Vector2){ sPack[i].body.pos.x,
                                    sPack[i].body.pos.y - BODY_H * 0.5f },
                         i, 0.0f,
                         (sPack[i].state == STALK_PROWL) ? 0.5f : 1.0f);
    }

    /* One at a time, and never in the first few seconds of a run. More of
       them come out after dark. */
    float rate = 0.004f * (1.0f + Boldness() * 2.2f);

    if (sElapsed > GRACE && alive < STALKER_MAX && Rand01() < rate)
    {
        TrySpawn(catPos.x);
    }
}

/* --- drawing -----------------------------------------------------------
   Authored facing LEFT, like everything else. The eyes are drawn in a
   second pass and glow, so in the dark you see those before you see the
   shape - the way it was introduced. */

#define SPR_W 30
#define SPR_H 18
#define SPR_PIXEL 1.5f

static const char *const SPRITE[SPR_H] = {
    "..KK.....KK...................",
    ".KLLK...KLLK..................",
    ".KLLLLLLLLLK..................",
    "KLFFLLLLLFFLK.................",
    "KLEFFFFFFFFLKKKK..............",
    "KFFFFFFFFFFFLLLLKK............",
    "KDFFFFFFFFFFLLLLLLK...........",
    ".KDFFFFFFFFFFFFFLLLLK.........",
    "..KDFFFFFFFFFFFFFFLLLLK.......",
    "...KDFFFFFFFFFFFFFFFLLLLK.....",
    "....KDFFFFFFFFFFFFFFFFFFFKTT..",
    ".....KFFDDDDDDDDDDDDFFDDK..TT.",
    ".....KK.KK.........KK.KK...TT.",
    ".....KFFK..........KFFK.......",
    ".....KFFK..........KFFK.......",
    ".....KFFK..........KFFK.......",
    ".....KDDK..........KDDK.......",
    ".....KKK...........KKK........",
};

static const Sprite STALKER_ART = { SPRITE, SPR_W, SPR_H, -1.0f };

/* The eyes are deliberately absent from this: the body pass leaves holes
   where they go, and the glow pass fills them at whatever brightness its
   interest has reached. */
static Color StalkerColor(char cell, const void *ctx)
{
    (void)ctx;

    switch (cell)
    {
        case 'K': return (Color){ 12, 11, 15, 255 };   /* outline */
        case 'D': return (Color){ 20, 18, 23, 255 };   /* dark    */
        case 'L': return (Color){ 58, 53, 62, 255 };   /* lit     */
        case 'T': return (Color){ 24, 22, 28, 255 };   /* tail    */
        case 'F': return (Color){ 34, 31, 39, 255 };   /* body    */
        default:  return BLANK;
    }
}

#define EYES_MAX 4

static void DrawEyes(const Stalker *s, Vector2 at)
{
    Vector2 eyes[EYES_MAX];
    int found = SpriteCells(STALKER_ART, at, SPR_PIXEL, s->facing,
                            'E', eyes, EYES_MAX);
    if (found > EYES_MAX) found = EYES_MAX;

    /* Brighter the more interested it is, with a soft bloom around each
       so they carry at a distance. */
    float heat = 0.35f + s->interest * 0.65f;
    Color glow = (Color){ 226, 132, 48, 255 };

    float p = SPR_PIXEL;

    for (int i = 0; i < found; i++)
    {
        DrawRectangleRec((Rectangle){ eyes[i].x - p, eyes[i].y - p,
                                      p * 3.0f, p * 3.0f },
                         Fade(glow, 0.16f * heat));

        DrawRectangleRec((Rectangle){ eyes[i].x, eyes[i].y, p, p * 2.0f },
                         Fade(glow, heat));
    }
}

void StalkersDraw(float alpha, float left, float right)
{
    /* Enough to cover the sprite and the bloom that spills past it. */
    float margin = SpriteHalfWidth(STALKER_ART, SPR_PIXEL) + 40.0f;

    for (int i = 0; i < STALKER_MAX; i++)
    {
        if (!sPack[i].active) continue;

        Vector2 at = BodyRenderPos(&sPack[i].body, alpha);
        if (at.x < left - margin || at.x > right + margin) continue;

        /* Hunting raises the head and lengthens the stride. */
        bool hunting = (sPack[i].state == STALK_HUNT ||
                        sPack[i].state == STALK_STRIKE);
        if (hunting) at.y -= SPR_PIXEL;

        SpriteDrawStanding(STALKER_ART, StalkerColor, NULL, at, SPR_PIXEL,
                           sPack[i].facing, 1.0f);

        DrawEyes(&sPack[i], at);
    }
}
