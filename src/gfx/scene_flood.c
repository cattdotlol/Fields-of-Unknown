#include "gfx/scene_flood.h"

#include "raylib.h"

#include <math.h>

#define RAIN_COUNT  340
#define RIPPLE_MAX   28
#define STAR_COUNT  150
#define STEAM_COUNT  14

#define WATER_LINE 0.63f    /* fraction of screen height */
#define POD_X      0.46f    /* fraction of screen width at cameraX = 0 */

typedef struct Drop {
    float x, y;             /* normalised */
    float speed, len;
} Drop;

typedef struct Ripple {
    float x, y;
    float life;
} Ripple;

typedef struct Star {
    float x, y;
    float phase;
    bool  big;
} Star;

/* Parallax layers. Buildings are never stored: each slot's shape comes
   from hashing its index, so the town extends as far as the world does
   and costs nothing to keep. */
typedef struct Layer {
    float parallax;     /* 0 = pinned to the screen, 1 = moves with the world */
    float spacing;      /* world units between building slots */
    float minH, maxH;   /* fraction of screen height */
    bool  detail;       /* chimneys and rooftop growth */
} Layer;

static const Layer LAYER_FAR  = { 0.10f, 190.0f, 0.06f, 0.22f, false };
static const Layer LAYER_NEAR = { 0.28f, 260.0f, 0.12f, 0.40f, true  };

static Drop   sRain[RAIN_COUNT];
static Ripple sRipples[RIPPLE_MAX];
static Star   sStars[STAR_COUNT];

static float sTime;
static float sFlicker;

/* Also its own: see the note in weather.c. */
static unsigned int sRng = 1u;

static float Rand01(void)
{
    sRng = sRng * 1664525u + 1013904223u;
    return (float)((sRng >> 8) & 0xFFFFFFu) / (float)0xFFFFFFu;
}

void FloodSceneInit(unsigned int seed)
{
    sRng = (seed ^ 0x5EED5EEDu) | 1u;

    for (int i = 0; i < RAIN_COUNT; i++)
    {
        sRain[i].x = Rand01();
        sRain[i].y = Rand01();
        sRain[i].speed = 0.55f + Rand01() * 0.95f;
        sRain[i].len = 0.012f + Rand01() * 0.030f;
    }

    for (int i = 0; i < RIPPLE_MAX; i++) sRipples[i].life = 0.0f;

    for (int i = 0; i < STAR_COUNT; i++)
    {
        sStars[i].x = Rand01();
        sStars[i].y = Rand01() * (WATER_LINE * 0.72f);
        sStars[i].phase = Rand01() * 6.28f;
        sStars[i].big = (Rand01() > 0.88f);
    }

    sTime = 0.0f;
}

float FloodSceneWaterY(void)
{
    return (float)GetScreenHeight() * WATER_LINE;
}

float FloodScenePodX(void)
{
    return POD_X;
}

void FloodSceneUpdate(float dt)
{
    sTime += dt;

    for (int i = 0; i < RAIN_COUNT; i++)
    {
        sRain[i].y += sRain[i].speed * dt;
        sRain[i].x += sRain[i].speed * dt * 0.10f;

        if (sRain[i].y > WATER_LINE)
        {
            for (int r = 0; r < RIPPLE_MAX; r++)
            {
                if (sRipples[r].life > 0.0f) continue;

                sRipples[r].x = sRain[i].x;
                sRipples[r].y = WATER_LINE + Rand01() * (1.0f - WATER_LINE) * 0.55f;
                sRipples[r].life = 1.0f;
                break;
            }

            sRain[i].y = -sRain[i].len;
            sRain[i].x = Rand01();
        }

        if (sRain[i].x > 1.0f) sRain[i].x -= 1.0f;
    }

    for (int i = 0; i < RIPPLE_MAX; i++)
    {
        if (sRipples[i].life > 0.0f) sRipples[i].life -= dt * 1.4f;
    }

    sFlicker = 0.55f + 0.45f * sinf(sTime * 9.0f) * sinf(sTime * 2.3f);
    if (sFlicker < 0.0f) sFlicker = 0.0f;
}

static Color Mul(Color c, float k)
{
    return (Color){ (unsigned char)((float)c.r * k),
                    (unsigned char)((float)c.g * k),
                    (unsigned char)((float)c.b * k), c.a };
}

static float CellSize(void)
{
    float px = floorf((float)GetScreenHeight() / 720.0f);
    return (px < 1.0f) ? 1.0f : px;
}

/* --- endless town ------------------------------------------------------ */

static unsigned int SlotHash(unsigned int layer, int slot, unsigned int salt)
{
    unsigned int h = (unsigned int)slot * 2654435761u ^ (layer + 1u) * 2246822519u
                     ^ (salt + 0x9E3779B9u);
    h ^= h >> 13;
    h *= 3266489917u;
    h ^= h >> 16;
    return h;
}

static float Slot01(unsigned int layer, int slot, unsigned int salt)
{
    return (float)(SlotHash(layer, slot, salt) & 0xFFFFu) / 65535.0f;
}

static void DrawLayer(const Layer *L, unsigned int id, Color color,
                      float cameraX, float waterY, float reveal, bool reflect)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float scroll = cameraX * L->parallax;

    int first = (int)floorf((scroll - L->spacing) / L->spacing);
    int last  = (int)ceilf((scroll + w + L->spacing) / L->spacing);

    for (int slot = first; slot <= last; slot++)
    {
        float x = (float)slot * L->spacing - scroll;
        float bw = L->spacing * (0.45f + Slot01(id, slot, 1u) * 0.50f);
        float bh = (L->minH + Slot01(id, slot, 2u) * (L->maxH - L->minH)) * h;

        DrawRectangle((int)x, (int)(waterY - bh), (int)bw, (int)bh, color);

        if (L->detail && Slot01(id, slot, 3u) > 0.55f)
        {
            float sw = bw * (0.14f + Slot01(id, slot, 4u) * 0.14f);
            float sh = bh * (0.30f + Slot01(id, slot, 5u) * 0.50f);
            float sx = x + Slot01(id, slot, 6u) * (bw - sw);

            DrawRectangle((int)sx, (int)(waterY - bh - sh), (int)sw, (int)sh, color);
        }

        if (L->detail)
        {
            int clumps = (int)(Slot01(id, slot, 7u) * 4.0f);

            for (int g = 0; g < clumps; g++)
            {
                unsigned int k = 20u + (unsigned int)g;
                float gx = x + Slot01(id, slot, k) * bw;
                float gw = 4.0f + Slot01(id, slot, k + 40u) * 12.0f;
                float gh = 8.0f + Slot01(id, slot, k + 80u) * 26.0f;

                Color c = (Slot01(id, slot, k + 120u) > 0.72f)
                        ? (Color){ 58, 30, 70, 255 } : (Color){ 28, 50, 32, 255 };

                DrawRectangle((int)gx, (int)(waterY - bh - gh), (int)gw, (int)gh,
                              Mul(c, reveal));
            }

            /* Roughly every thirteenth building still has a light. The
               old version picked a single slot near the origin, so in an
               endless world you walked past it once and never saw
               another. */
            if ((SlotHash(id, slot, 99u) % 13u) == 0u)
            {
                float cell = CellSize();
                DrawRectangle((int)(x + bw * 0.5f), (int)(waterY - bh * 0.6f),
                              (int)(cell * 2.0f), (int)(cell * 2.0f),
                              Mul((Color){ 220, 150, 70, 255 }, reveal * sFlicker));
            }
        }

        if (reflect)
        {
            float rh = bh * 0.55f;
            float wobble = sinf(sTime * 1.3f + (float)slot) * 3.0f;

            DrawRectangle((int)(x + wobble), (int)waterY, (int)bw, (int)rh,
                          Mul((Color){ 16, 28, 32, 255 }, reveal * 0.9f));
        }
    }
}

/* Rasterised on the pixel grid so sky bodies belong to the same world. */
static void DrawPixelDisc(float cx, float cy, float r, float cell, Color base, Color band)
{
    float top = floorf((cy - r) / cell) * cell;

    for (float y = top; y <= cy + r; y += cell)
    {
        float dy = y - cy;
        if (fabsf(dy) > r) continue;

        float half = sqrtf(r * r - dy * dy);
        float x0 = floorf((cx - half) / cell) * cell;
        float width = floorf((half * 2.0f) / cell) * cell;
        if (width < cell) continue;

        int rowIndex = (int)((y - (cy - r)) / cell);
        bool banded = ((rowIndex / 3) % 2) == 0;

        DrawRectangle((int)x0, (int)y, (int)width, (int)cell, banded ? base : band);
    }
}

#define POD_W 20
#define POD_H 9

/* 'X' hull, 'O' scorched panel, 'H' the open hatch it crawled out of. */
static const char *POD[POD_H] = {
    ".....XXXXXXXXX......",
    "...XXOOOOOOOOOXX....",
    "..XXOOOOOOOOOOOXX...",
    ".XXOOOHHHHOOOOOOXX..",
    ".XXOOOHHHHOOOOOOOXX.",
    "..XXOOHHHHOOOOOOOXX.",
    "...XXOOOOOOOOOOOXX..",
    "....XXXXXXXXXXXXX...",
    ".....XX.......XX....",
};

static void DrawPod(float reveal, float w, float waterY, float cell, float cameraX)
{
    float scale = cell * 3.0f;
    float x = floorf(POD_X * w - cameraX * LAYER_NEAR.parallax);
    float y = floorf(waterY - (float)POD_H * scale + scale * 3.0f);

    /* Off screen once you have walked far enough from the crash site. */
    if (x + (float)POD_W * scale < 0.0f || x > w) return;

    Color hull  = Mul((Color){ 38, 42, 50, 255 }, reveal);
    Color panel = Mul((Color){ 20, 23, 30, 255 }, reveal);
    Color hatch = Mul((Color){ 6, 8, 12, 255 }, reveal);

    for (int row = 0; row < POD_H; row++)
    {
        for (int col = 0; col < POD_W; col++)
        {
            char c = POD[row][col];
            if (c == '.') continue;

            float py = y + (float)row * scale;
            if (py > waterY) continue;

            Color use = (c == 'X') ? hull : ((c == 'O') ? panel : hatch);

            DrawRectangle((int)(x + (float)col * scale), (int)py,
                          (int)scale, (int)scale, use);
        }
    }

    for (int i = 0; i < STEAM_COUNT; i++)
    {
        float t = sTime * 0.5f + (float)i * 0.37f;
        float rise = fmodf(t, 1.0f);
        float sx = x + scale * (5.0f + (float)(i % 5) * 2.0f)
                     + sinf(t * 4.0f + (float)i) * scale * 1.5f;
        float sy = y - rise * scale * 10.0f;

        DrawRectangle((int)sx, (int)sy, (int)cell, (int)cell,
                      Fade(Mul((Color){ 150, 160, 165, 255 }, reveal), (1.0f - rise) * 0.22f));
    }
}

void FloodSceneDraw(float reveal, float rain, float cameraX, bool snow)
{
    if (reveal < 0.0f) reveal = 0.0f;
    if (reveal > 1.0f) reveal = 1.0f;
    if (rain < 0.0f) rain = 0.0f;
    if (rain > 1.0f) rain = 1.0f;

    int drops = (int)(RAIN_COUNT * rain);

    int w = GetScreenWidth();
    int h = GetScreenHeight();
    float fw = (float)w;
    float fh = (float)h;
    float waterY = FloodSceneWaterY();
    float cell = CellSize();

    /* Sky: violet-black overhead washing to a sick teal at the horizon. */
    DrawRectangleGradientV(0, 0, w, (int)waterY,
                           Mul((Color){ 16, 12, 30, 255 }, reveal),
                           Mul((Color){ 34, 52, 50, 255 }, reveal));

    /* Stars barely move; they are supposed to be very far away. */
    float starScroll = cameraX * 0.012f;
    for (int i = 0; i < STAR_COUNT; i++)
    {
        float tw = 0.45f + 0.55f * sinf(sTime * 1.4f + sStars[i].phase);
        tw = floorf(tw * 4.0f) / 4.0f;

        float sx = fmodf(sStars[i].x * fw - starScroll, fw);
        if (sx < 0.0f) sx += fw;

        float side = sStars[i].big ? cell * 2.0f : cell;

        DrawRectangle((int)sx, (int)(sStars[i].y * fh), (int)side, (int)side,
                      Fade(Mul((Color){ 225, 230, 245, 255 }, reveal), tw * 0.7f));
    }

    DrawPixelDisc(fw * 0.76f - cameraX * 0.02f, fh * 0.20f, fh * 0.155f, cell,
                  Mul((Color){ 74, 60, 96, 255 }, reveal),
                  Mul((Color){ 58, 46, 78, 255 }, reveal));

    DrawPixelDisc(fw * 0.17f - cameraX * 0.03f, fh * 0.13f, fh * 0.035f, cell,
                  Mul((Color){ 150, 156, 170, 255 }, reveal),
                  Mul((Color){ 126, 132, 148, 255 }, reveal));

    DrawLayer(&LAYER_FAR, 0u, Mul((Color){ 26, 36, 44, 255 }, reveal),
              cameraX, waterY, reveal, false);

    DrawLayer(&LAYER_NEAR, 1u, Mul((Color){ 8, 12, 18, 255 }, reveal),
              cameraX, waterY, reveal, false);

    DrawPod(reveal, fw, waterY, cell, cameraX);

    DrawRectangle(0, (int)waterY, w, h - (int)waterY,
                  Mul((Color){ 12, 24, 28, 255 }, reveal));

    /* Reflections go on after the water, so they sit in it. */
    DrawLayer(&LAYER_NEAR, 1u, BLANK, cameraX, waterY, reveal, true);

    for (float y = waterY; y < fh; y += cell * 4.0f)
    {
        float depth = (y - waterY) / (fh - waterY);
        DrawRectangle(0, (int)y, w, (int)cell,
                      Fade(Mul((Color){ 32, 54, 58, 255 }, reveal), 0.10f + depth * 0.20f));
    }

    for (int i = 0; i < RIPPLE_MAX; i++)
    {
        if (sRipples[i].life <= 0.0f) continue;

        float grow = 1.0f - sRipples[i].life;
        float rw = cell * (3.0f + grow * 16.0f);

        DrawRectangle((int)(sRipples[i].x * fw - rw * 0.5f), (int)(sRipples[i].y * fh),
                      (int)rw, (int)cell,
                      Fade(Mul((Color){ 120, 175, 180, 255 }, reveal), sRipples[i].life * 0.35f));
    }

    for (int i = 0; i < drops; i++)
    {
        if (sRain[i].y > WATER_LINE) continue;

        float px = sRain[i].x * fw;
        float py = sRain[i].y * fh;

        if (snow)
        {
            /* Flakes drift and tumble instead of falling in streaks. */
            float drift = sinf(sTime * 0.8f + (float)i * 0.7f) * cell * 6.0f;

            DrawRectangle((int)(px + drift), (int)(py * 0.85f),
                          (int)(cell * 2.0f), (int)(cell * 2.0f),
                          Fade(Mul((Color){ 226, 234, 240, 255 }, reveal), 0.35f + rain * 0.35f));
        }
        else
        {
            DrawRectangle((int)px, (int)py, (int)cell, (int)(sRain[i].len * fh),
                          Fade(Mul((Color){ 150, 180, 190, 255 }, reveal), 0.18f + rain * 0.20f));
        }
    }
}
