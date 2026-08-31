#include "ui/cursor.h"
#include "ui/theme.h"

#include "raylib.h"

#include <math.h>
#include <stdbool.h>

#define CURSOR_W 12
#define CURSOR_H 18

/* 'X' outline, 'O' fill, '.' transparent. Hotspot is the top-left tip. */
static const char *ARROW[CURSOR_H] = {
    "X...........",
    "XX..........",
    "XOX.........",
    "XOOX........",
    "XOOOX.......",
    "XOOOOX......",
    "XOOOOOX.....",
    "XOOOOOOX....",
    "XOOOOOOOX...",
    "XOOOOOOOOX..",
    "XOOOOOOOOOX.",
    "XOOOOOOXXXXX",
    "XOOXOOX.....",
    "XOX.XOOX....",
    "XX..XOOX....",
    "X....XOOX...",
    ".....XOOX...",
    "......XX....",
};

#define TRAIL_LEN  7
#define RIPPLE_MAX 8

typedef struct Ripple {
    Vector2 pos;
    float   life;      /* 1 -> 0 */
} Ripple;

static Vector2 sTrail[TRAIL_LEN];
static int     sTrailHead;
static Ripple  sRipples[RIPPLE_MAX];
static bool    sHot;         /* over something clickable */
static bool    sHotNext;
static float   sPress;       /* 1 while held, eases back to 0 */
static float   sTime;

void CursorHint(void)
{
    sHotNext = true;
}

void CursorLoad(void)
{
    HideCursor();

    Vector2 m = GetMousePosition();
    for (int i = 0; i < TRAIL_LEN; i++) sTrail[i] = m;
}

/* One cursor pixel, in real screen pixels. */
static float Cell(void)
{
    return PixelScale();
}

void CursorUpdate(float dt)
{
    sTime += dt;

    sHot = sHotNext;
    sHotNext = false;

    sTrailHead = (sTrailHead + 1) % TRAIL_LEN;
    sTrail[sTrailHead] = GetMousePosition();

    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT)) sPress = 1.0f;
    else                                      sPress -= dt * 6.0f;
    if (sPress < 0.0f) sPress = 0.0f;

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        for (int i = 0; i < RIPPLE_MAX; i++)
        {
            if (sRipples[i].life > 0.0f) continue;

            sRipples[i].pos = GetMousePosition();
            sRipples[i].life = 1.0f;
            break;
        }
    }

    for (int i = 0; i < RIPPLE_MAX; i++)
    {
        if (sRipples[i].life > 0.0f) sRipples[i].life -= dt * 2.6f;
    }
}

static void DrawBitmap(Vector2 at, float cell, Color outline, Color fill)
{
    float x0 = floorf(at.x);
    float y0 = floorf(at.y);

    for (int row = 0; row < CURSOR_H; row++)
    {
        for (int col = 0; col < CURSOR_W; col++)
        {
            char c = ARROW[row][col];
            if (c == '.') continue;

            DrawRectangle((int)(x0 + (float)col * cell), (int)(y0 + (float)row * cell),
                          (int)cell, (int)cell, (c == 'X') ? outline : fill);
        }
    }
}

/* Square brackets that close in when the pointer is over a control. */
static void DrawHotBrackets(Vector2 at, float cell)
{
    float span = cell * 9.0f;
    float arm  = cell * 3.0f;
    float gap  = cell * (2.0f + sinf(sTime * 6.0f) * 0.6f);

    Vector2 c = { at.x + cell * 4.0f, at.y + cell * 6.0f };
    Color col = Fade(gTheme.accent, 0.85f);

    float l = c.x - span - gap;
    float r = c.x + span + gap;
    float t = c.y - span - gap;
    float b = c.y + span + gap;

    /* four corners, two arms each */
    DrawRectangle((int)l, (int)t, (int)arm, (int)cell, col);
    DrawRectangle((int)l, (int)t, (int)cell, (int)arm, col);

    DrawRectangle((int)(r - arm), (int)t, (int)arm, (int)cell, col);
    DrawRectangle((int)(r - cell), (int)t, (int)cell, (int)arm, col);

    DrawRectangle((int)l, (int)(b - cell), (int)arm, (int)cell, col);
    DrawRectangle((int)l, (int)(b - arm), (int)cell, (int)arm, col);

    DrawRectangle((int)(r - arm), (int)(b - cell), (int)arm, (int)cell, col);
    DrawRectangle((int)(r - cell), (int)(b - arm), (int)cell, (int)arm, col);
}

void CursorDraw(void)
{
    if (!IsCursorOnScreen()) return;

    float cell = Cell();
    Vector2 m = GetMousePosition();

    /* Motion trail: oldest sample first, so newer squares draw on top. */
    for (int i = 1; i < TRAIL_LEN; i++)
    {
        int idx = (sTrailHead + i) % TRAIL_LEN;
        float age = (float)i / (float)TRAIL_LEN;

        float side = floorf(cell * (0.5f + age * 0.5f));
        if (side < 1.0f) side = 1.0f;

        DrawRectangle((int)(sTrail[idx].x + cell), (int)(sTrail[idx].y + cell),
                      (int)side, (int)side, Fade(gTheme.accent, age * 0.28f));
    }

    for (int i = 0; i < RIPPLE_MAX; i++)
    {
        if (sRipples[i].life <= 0.0f) continue;

        float grow = (1.0f - sRipples[i].life);
        float half = floorf(cell * (2.0f + grow * 9.0f));
        Rectangle r = { floorf(sRipples[i].pos.x - half), floorf(sRipples[i].pos.y - half),
                        half * 2.0f, half * 2.0f };

        DrawRectangleLinesEx(r, cell * 0.5f, Fade(gTheme.accent, sRipples[i].life * 0.7f));
    }

    if (sHot) DrawHotBrackets(m, cell);

    /* Pressing sinks the cursor by a pixel - cheap, readable feedback. */
    float sink = (sPress > 0.5f) ? cell : 0.0f;
    Vector2 at = { m.x + sink, m.y + sink };

    Color outline = (Color){ 8, 10, 20, 255 };
    Color fill    = sHot ? gTheme.text : gTheme.accent;
    if (sPress > 0.5f) fill = WHITE;

    DrawBitmap(at, cell, outline, fill);
}
