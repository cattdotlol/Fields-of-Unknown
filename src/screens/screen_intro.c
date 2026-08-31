#include "screens/screens.h"
#include "core/app.h"
#include "core/config.h"
#include "entity/cat_art.h"
#include "gfx/filmfx.h"
#include "gfx/scene_flood.h"
#include "ui/theme.h"

#include "world/daylight.h"

#include "raylib.h"

#include <math.h>

/* Cold open: the cat comes down from orbit onto a planet nobody charted.
   It establishes the wreck and the place, and nothing else. Hunger and
   the food chain are for the player to walk into on their own - saying
   either out loud here would be a tutorial in narration. The one hint
   is something in the dark that opens its eyes and does not explain
   itself. */

typedef struct Beat {
    float start;
    float dur;
    const char *line;
} Beat;

static const Beat BEATS[] = {
    {  0.8f, 3.6f, "YOU WERE ASLEEP WHEN THE SHIP CAME APART." },
    {  4.8f, 3.8f, "SOMETHING PUT YOU DOWN HERE." },
    {  9.2f, 4.0f, "THE CHARTS DO NOT REACH THIS FAR." },
};

#define BEAT_COUNT ((int)(sizeof(BEATS) / sizeof(BEATS[0])))

#define FADE_TIME     0.9f
#define TYPE_TIME     0.9f

#define DESCENT_IN    1.4f    /* the pod streaks down */
#define IMPACT        4.0f    /* white-out */
#define SCENE_IN      4.4f    /* the planet fades up */
#define SCENE_FULL    9.6f
#define EYES_OPEN    13.4f

/* Something else is already awake out there. It never gets a line. */
#define WATCH_IN     15.4f
#define WATCH_OUT    18.2f

#define OUTRO_START  19.4f
#define OUTRO_END    21.0f

#define SPACE_STARS 130

static float   sTime;
static Vector2 sSpace[SPACE_STARS];
static float   sSpaceDrift[SPACE_STARS];

static void Init(void)
{
    sTime = 0.0f;
    FloodSceneInit(WORLD_SEED);

    /* The cat comes down at first light, however long the player sat on
       the title screen watching the sky move. */
    DaylightInit();

    for (int i = 0; i < SPACE_STARS; i++)
    {
        sSpace[i].x = (float)GetRandomValue(0, 1000) / 1000.0f;
        sSpace[i].y = (float)GetRandomValue(0, 1000) / 1000.0f;
        sSpaceDrift[i] = 0.004f + (float)GetRandomValue(0, 100) / 6000.0f;
    }
}

static void Update(float dt)
{
    sTime += dt;

    FloodSceneUpdate(dt);

    for (int i = 0; i < SPACE_STARS; i++)
    {
        sSpace[i].y += sSpaceDrift[i] * dt;
        if (sSpace[i].y > 1.0f) sSpace[i].y -= 1.0f;
    }

    if (GetKeyPressed() != 0 || IsMouseButtonPressed(MOUSE_BUTTON_LEFT) ||
        sTime >= OUTRO_END)
    {
        AppGoTo(SCREEN_GAMEPLAY);
    }
}

static float Ramp(float t, float from, float to)
{
    if (t <= from) return 0.0f;
    if (t >= to) return 1.0f;

    return (t - from) / (to - from);
}

static float BeatAlpha(const Beat *b, float t)
{
    if (t < b->start || t > b->start + b->dur) return 0.0f;

    float e = t - b->start;
    if (e < FADE_TIME) return e / FADE_TIME;
    if (e > b->dur - FADE_TIME) return (b->dur - e) / FADE_TIME;

    return 1.0f;
}

static float Cell(void)
{
    return PixelScale();
}

/* Open on empty space, before the planet exists. */
static void DrawSpace(float alpha)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float cell = Cell();

    for (int i = 0; i < SPACE_STARS; i++)
    {
        float tw = 0.4f + 0.6f * sinf(sTime * 1.7f + (float)i);
        tw = floorf(tw * 4.0f) / 4.0f;

        DrawRectangle((int)(sSpace[i].x * w), (int)(sSpace[i].y * h),
                      (int)cell, (int)cell, Fade(RAYWHITE, tw * 0.75f * alpha));
    }
}

/* The pod coming in, with a burn trail behind it. */
static void DrawDescent(void)
{
    float progress = Ramp(sTime, DESCENT_IN, IMPACT);
    if (progress <= 0.0f || progress >= 1.0f) return;

    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float cell = Cell();

    for (int i = 0; i < 26; i++)
    {
        float back = progress - (float)i * 0.016f;
        if (back <= 0.0f) break;

        float x = (0.12f + back * 0.52f) * w;
        float y = (0.04f + back * back * 0.46f) * h;
        float fade = (1.0f - (float)i / 26.0f);

        float side = cell * (1.0f + fade * 2.0f);

        DrawRectangle((int)x, (int)y, (int)side, (int)side,
                      Fade((Color){ 255, 214, 150, 255 }, fade * 0.85f));
    }
}

static void DrawCat(float reveal)
{
    float cell = PixelScale() * 2.0f;

    float w = (float)GetScreenWidth();
    float waterY = FloodSceneWaterY();

    /* Sat just clear of the wreck it climbed out of. */
    float x = floorf((FloodScenePodX() - 0.125f) * w);
    float y = floorf(waterY - (float)CAT_ART_H * cell);

    DrawRectangle((int)(x - cell * 3.0f), (int)waterY,
                  (int)(cell * (CAT_ART_W + 6)), (int)(cell * 2.0f),
                  Fade((Color){ 7, 10, 14, 255 }, reveal));

    /* Eyes come open on cue, then blink irregularly. */
    float open = Ramp(sTime, EYES_OPEN, EYES_OPEN + 1.4f);

    /* Blink every 2.9s once the eyes are open, for long enough to see. */
    float since = sTime - (EYES_OPEN + 1.4f);
    if (since > 0.0f && fmodf(since, 2.9f) < 0.13f) open *= 0.08f;

    CatArtDraw(x, y, cell, cell, CAT_ART_AUTHORED_FACING, open, reveal, 0.0f);
}

/* Two eyes in the dark structures across the water - bigger than the
   cat's, and set further apart. They open, hold, and blink out. */
static void DrawWatcher(float reveal)
{
    float show = Ramp(sTime, WATCH_IN, WATCH_IN + 1.6f) *
                 (1.0f - Ramp(sTime, WATCH_OUT, WATCH_OUT + 0.18f));

    if (show <= 0.0f || reveal <= 0.0f) return;

    float w = (float)GetScreenWidth();
    float waterY = FloodSceneWaterY();
    float cell = Cell();

    float x = floorf(w * 0.735f);
    float y = floorf(waterY - cell * 9.0f);

    float side = cell * 2.0f;
    float gap = cell * 7.0f;

    Color glow = Fade((Color){ 206, 96, 58, 255 }, reveal * show * 0.9f);

    DrawRectangle((int)x, (int)y, (int)side, (int)side, glow);
    DrawRectangle((int)(x + gap), (int)y, (int)side, (int)side, glow);
}

static void Draw(void)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float s = ThemeScale();

    float reveal = Ramp(sTime, SCENE_IN, SCENE_FULL);
    float space = 1.0f - Ramp(sTime, IMPACT, SCENE_IN + 1.2f);

    ClearBackground(BLACK);

    FloodSceneDraw(reveal, 0.85f, 0.0f, false);
    DrawWatcher(reveal);
    DrawCat(reveal);

    if (space > 0.0f)
    {
        DrawSpace(space);
        DrawDescent();
    }

    /* Whiteout on impact, decaying fast. */
    float flash = 1.0f - Ramp(sTime, IMPACT, IMPACT + 0.55f);
    if (sTime >= IMPACT - 0.05f && flash > 0.0f)
    {
        DrawRectangle(0, 0, (int)w, (int)h, Fade(RAYWHITE, flash * 0.9f));
    }

    FilmVignette(0.9f);
    FilmScanlines(0.55f);
    FilmGrain(0.5f);
    FilmLetterbox(Ramp(sTime, 0.0f, 1.2f));

    for (int i = 0; i < BEAT_COUNT; i++)
    {
        float a = BeatAlpha(&BEATS[i], sTime);
        if (a <= 0.0f) continue;

        float elapsed = sTime - BEATS[i].start;
        int total = TextLength(BEATS[i].line);
        int shown = (int)((elapsed / TYPE_TIME) * (float)total);
        if (shown > total) shown = total;
        if (shown < 0) shown = 0;

        UiTextCentered(TextSubtext(BEATS[i].line, 0, shown), w * 0.5f, h - 150.0f * s,
                       20.0f * s, Fade(gTheme.text, a));
    }

    float out = Ramp(sTime, OUTRO_START, OUTRO_END);
    if (out > 0.0f) DrawRectangle(0, 0, (int)w, (int)h, Fade(BLACK, out));

    float hint = Ramp(sTime, 1.8f, 3.0f) * (1.0f - out);
    if (hint > 0.0f)
    {
        const char *skip = "ANY KEY TO SKIP";
        Vector2 m = UiMeasure(skip, 10.0f * s);

        UiText(skip, w - m.x - 40.0f * s, h - 46.0f * s, 10.0f * s,
               Fade(gTheme.textDim, hint * 0.7f));
    }
}

const Screen ScreenIntro = {
    .init = Init,
    .update = Update,
    .draw = Draw,
    .unload = NULL,
    .opaque = true,
    .hidesCursor = true,
};
