#include "ui/hud.h"
#include "entity/vitals.h"
#include "ui/theme.h"
#include "world/season.h"
#include "world/weather.h"

#include "raylib.h"

#include <math.h>

#define BAR_SEGMENTS 20

#define HEART_W     7
#define HEART_H     6
#define HEART_COUNT 5

/* R body, H shine. Drawn rather than stored, like everything else. */
static const char *HEART[HEART_H] = {
    ".RR.RR.",
    "RHRRRRR",
    "RRRRRRR",
    ".RRRRR.",
    "..RRR..",
    "...R...",
};

/* fill: 1 full, 0.5 half, 0 empty. Halves split down the middle so a
   glance reads the same way it does in every game that does this. */
static void DrawHeart(float x, float y, float cell, float fill, Color tint)
{
    Color shadow = (Color){ 8, 8, 12, 190 };
    Color empty  = (Color){ 44, 40, 50, 255 };
    Color shine  = (Color){ 236, 152, 152, 255 };

    for (int row = 0; row < HEART_H; row++)
    {
        for (int col = 0; col < HEART_W; col++)
        {
            char c = HEART[row][col];
            if (c == '.') continue;

            float px = x + (float)col * cell;
            float py = y + (float)row * cell;

            /* Offset copy underneath, so hearts read against any sky. */
            DrawRectangleRec((Rectangle){ px + cell, py + cell, cell, cell }, shadow);

            bool lit = (fill >= 1.0f) || (fill >= 0.5f && col <= 2);

            Color use = empty;
            if (lit) use = (c == 'H') ? shine : tint;

            DrawRectangleRec((Rectangle){ px, py, cell, cell }, use);
        }
    }
}

static void Bar(float x, float y, float w, float h, float value, Color fill, const char *label)
{
    float s = ThemeScale();
    float px = (s < 1.0f) ? 1.0f : floorf(s);

    Color track = (Color){ 18, 20, 28, 210 };

    UiText(label, x, y - 12.0f * s, 10.0f * s, gTheme.textDim);

    DrawRectangleRec((Rectangle){ x, y, w, h }, track);

    float segW = w / (float)BAR_SEGMENTS;
    int filled = (int)(value * (float)BAR_SEGMENTS + 0.5f);

    for (int i = 0; i < filled; i++)
    {
        DrawRectangleRec((Rectangle){ x + (float)i * segW, y, segW - px, h }, fill);
    }

    DrawRectangleLinesEx((Rectangle){ x, y, w, h }, px, gTheme.border);
}

/* Anchored bottom-left. The camera keeps the cat just above centre, so
   the top of the screen is where the world is; the bottom corner is the
   quietest part of the frame. */
#define HUD_MARGIN 30.0f

void HudDraw(void)
{
    float s = ThemeScale();

    float w = 170.0f * s;
    float h = 11.0f * s;
    float step = 27.0f * s;
    float cellPreview = 3.0f * s;

    float blockH = (float)HEART_H * cellPreview + 24.0f * s + step * 3.0f + 14.0f * s;

    float x = HUD_MARGIN * s;
    float y = (float)GetScreenHeight() - HUD_MARGIN * s - blockH;

    Color health  = (Color){ 198,  72,  72, 255 };
    Color hunger  = (Color){ 208, 150,  60, 255 };
    Color stamina = (Color){  96, 220, 232, 255 };
    Color warmth  = (Color){ 150, 190, 220, 255 };

    /* Low health pulses brighter, not fainter: fading the alpha would
       show the heart's own drop shadow through it. */
    if (gVitals.health < 0.3f)
    {
        float beat = 0.5f + 0.5f * sinf((float)GetTime() * 6.0f);
        health = ColorLerp(health, (Color){ 244, 120, 120, 255 }, beat);
    }

    /* Health is hearts: five of them, in halves. */
    float cell = cellPreview;
    float hearts = gVitals.health * (float)HEART_COUNT;
    float advance = (float)(HEART_W + 2) * cell;

    for (int i = 0; i < HEART_COUNT; i++)
    {
        float value = hearts - (float)i;
        if (value < 0.0f) value = 0.0f;
        if (value > 1.0f) value = 1.0f;

        /* Snap to halves so a heart is never ambiguously part-full. */
        value = floorf(value * 2.0f + 0.5f) * 0.5f;

        DrawHeart(x + (float)i * advance, y, cell, value, health);
    }

    float below = y + (float)HEART_H * cell + 24.0f * s;

    Bar(x, below,            w, h, gVitals.hunger,  hunger,  "FOOD");
    Bar(x, below + step,     w, h, gVitals.stamina, stamina, "STAMINA");
    Bar(x, below + step * 2, w, h, gVitals.warmth,  warmth,  "WARMTH");

    /* Season and sky, which are what is doing this to the rest. */
    const char *line = TextFormat("%s   %s", SeasonName(), WeatherName());
    UiText(line, x, below + step * 3 + 4.0f * s, 10.0f * s, gTheme.accent);
}
