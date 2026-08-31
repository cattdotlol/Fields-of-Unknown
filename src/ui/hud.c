#include "ui/hud.h"
#include "entity/vitals.h"
#include "ui/theme.h"
#include "world/season.h"
#include "world/weather.h"

#include "raylib.h"

#include <math.h>

#define BAR_SEGMENTS 20

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

void HudDraw(void)
{
    float s = ThemeScale();

    float x = 26.0f * s;
    float y = 34.0f * s;
    float w = 190.0f * s;
    float h = 12.0f * s;
    float step = 30.0f * s;

    Color health  = (Color){ 198,  84,  84, 255 };
    Color hunger  = (Color){ 208, 150,  60, 255 };
    Color stamina = (Color){  96, 220, 232, 255 };
    Color warmth  = (Color){ 150, 190, 220, 255 };

    /* Low health pulses, so it is noticed without a number. */
    if (gVitals.health < 0.3f)
    {
        float beat = 0.6f + 0.4f * sinf((float)GetTime() * 6.0f);
        health = Fade(health, beat);
    }

    Bar(x, y,             w, h, gVitals.health,  health,  "HEALTH");
    Bar(x, y + step,      w, h, gVitals.hunger,  hunger,  "FOOD");
    Bar(x, y + step * 2,  w, h, gVitals.stamina, stamina, "STAMINA");
    Bar(x, y + step * 3,  w, h, gVitals.warmth,  warmth,  "WARMTH");

    /* Season and sky, which are what is doing this to the bars above. */
    const char *line = TextFormat("%s   %s", SeasonName(), WeatherName());
    UiText(line, x, y + step * 4 + 4.0f * s, 10.0f * s, gTheme.accent);
}
