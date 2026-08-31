#include "screens/screens.h"
#include "core/app.h"
#include "core/config.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include "raylib.h"

#include <math.h>

enum { ITEM_START = 0, ITEM_SETTINGS, ITEM_QUIT, ITEM_COUNT };

static const char *ITEMS[ITEM_COUNT] = { "START", "SETTINGS", "QUIT" };

static int   sSelected;
static float sTime;

static void Init(void)
{
    sSelected = 0;
    sTime = 0.0f;
}

static void Update(float dt)
{
    sTime += dt;
}

/* Hard offset drop shadow instead of a blur - a soft glow would fight
   the pixel grid. */
static void DrawTitleText(float cx, float y, float size)
{
    float px = floorf(ThemeScale());
    if (px < 1.0f) px = 1.0f;

    float offset = floorf(size * 0.06f / px) * px;

    UiTextCentered(GAME_TITLE_DISPLAY, cx + offset, y + offset, size, (Color){ 20, 24, 48, 255 });
    UiTextCentered(GAME_TITLE_DISPLAY, cx, y, size, gTheme.accent);
    UiTextCentered(GAME_TITLE_DISPLAY, cx - px, y - px, size, gTheme.text);
}

/* The logo is the widest thing on screen, so step the size down until it
   fits rather than letting it run off the edges on a narrow window. */
static float FitTitleSize(float maxWidth)
{
    float s = ThemeScale();

    for (float design = 80.0f; design > 30.0f; design -= 10.0f)
    {
        if (UiMeasure(GAME_TITLE_DISPLAY, design * s).x <= maxWidth) return design * s;
    }

    return 30.0f * s;
}

static void Draw(void)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float s = ThemeScale();
    float cx = w * 0.5f;

    float titleSize = FitTitleSize(w * 0.86f);
    float titleY = floorf(h * 0.18f);

    DrawTitleText(cx, titleY, titleSize);

    float ruleW = floorf(300.0f * s);
    float ruleY = floorf(titleY + UiLineHeight(titleSize) * 1.1f);
    float px = (s < 1.0f) ? 1.0f : floorf(s);

    DrawRectangle((int)(cx - ruleW * 0.5f), (int)ruleY, (int)ruleW, (int)(2.0f * px),
                  Fade(gTheme.accent, 0.7f));

    UiTextCentered(GAME_TAGLINE, cx, ruleY + 18.0f * s, 10.0f * s, gTheme.textDim);

    int activated = WidgetMenu(ITEMS, ITEM_COUNT, &sSelected, cx, floorf(h * 0.50f), 30.0f * s);

    switch (activated)
    {
        case ITEM_START:    AppGoTo(SCREEN_INTRO); break;
        case ITEM_SETTINGS: AppGoTo(SCREEN_SETTINGS); break;
        case ITEM_QUIT:     AppQuit(); break;
        default: break;
    }

    UiTextCentered("UP / DOWN  NAVIGATE      ENTER  SELECT", cx, h - 50.0f * s, 10.0f * s,
                   Fade(gTheme.textDim, 0.8f));
}

const Screen ScreenTitle = { .init = Init, .update = Update, .draw = Draw, .unload = NULL };
