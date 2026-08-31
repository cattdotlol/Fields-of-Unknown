#include "core/app.h"
#include "core/config.h"
#include "core/input.h"
#include "core/settings.h"
#include "core/sysinfo.h"
#include "gfx/filmfx.h"
#include "gfx/scene_flood.h"
#include "world/weather.h"
#include "ui/theme.h"
#include "ui/cursor.h"

#include "raylib.h"

#define FADE_SPEED 3.2f

/* The simulation runs at a fixed rate no matter what the display does. */
#define TICK_HZ      60
#define TICK_DT      (1.0f / (float)TICK_HZ)
#define MAX_CATCHUP  5      /* ticks per frame before we drop the backlog */
#define MAX_FRAME    0.25f  /* clamp, so a stall cannot spiral */

/* Registry index must match the ScreenId enum order. */
static const Screen *sScreens[SCREEN_COUNT];

static ScreenId sCurrent = SCREEN_NONE;
static ScreenId sPending = SCREEN_NONE;
static float    sFade;          /* 0 = clear, 1 = black */
static int      sFadeDir;       /* +1 out, -1 in, 0 idle */
static bool     sQuit;
static float    sAccumulator;
static float    sAlpha;

static void EnterScreen(ScreenId id)
{
    if (sCurrent != SCREEN_NONE && sScreens[sCurrent]->unload) sScreens[sCurrent]->unload();

    sCurrent = id;

    if (sCurrent != SCREEN_NONE && sScreens[sCurrent]->init) sScreens[sCurrent]->init();
}

void AppGoTo(ScreenId id)
{
    if (sFadeDir != 0 || id == sCurrent) return;

    sPending = id;
    sFadeDir = 1;
}

void AppQuit(void)
{
    sQuit = true;
}

float AppRenderAlpha(void)
{
    return sAlpha;
}

void AppInit(void)
{
    /* No MSAA: smoothed edges would undo the pixel-sharp look. */
    SetConfigFlags(FLAG_WINDOW_RESIZABLE | FLAG_VSYNC_HINT | FLAG_WINDOW_MAXIMIZED);
    InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, GAME_TITLE);
    SetWindowMinSize(960, 540);

    /* Maximised fills the desktop work area, so panels and taskbars stay
       visible - unlike ToggleFullscreen(), which covers them. The config
       flag avoids a visible resize pop; this is the fallback for window
       managers that ignore the hint. WINDOW_WIDTH/HEIGHT remain the
       restore size when the user un-maximises. */
    if (!IsWindowMaximized()) MaximizeWindow();
    InitAudioDevice();
    SetExitKey(KEY_NULL);          /* screens decide what ESC means */
    SetTargetFPS(60);

    InputInit();

    SettingsDefaults();
    SettingsLoad(SETTINGS_FILE);
    SettingsApply();

    ThemeLoad();
    CursorLoad();
    SysInfoGather();          /* needs the GL context InitWindow created */
    FloodSceneInit(WORLD_SEED);
    WeatherInit(WORLD_SEED);

    sScreens[SCREEN_TITLE]    = &ScreenTitle;
    sScreens[SCREEN_INTRO]    = &ScreenIntro;
    sScreens[SCREEN_SETTINGS] = &ScreenSettings;
    sScreens[SCREEN_GAMEPLAY] = &ScreenGameplay;

    EnterScreen(SCREEN_TITLE);
}

void AppRun(void)
{
    while (!WindowShouldClose() && !sQuit)
    {
        float frame = GetFrameTime();
        if (frame > MAX_FRAME) frame = MAX_FRAME;

        InputPoll();

        /* --- fixed-rate simulation ------------------------------------ */
        sAccumulator += frame;

        int ticks = 0;
        while (sAccumulator >= TICK_DT && ticks < MAX_CATCHUP)
        {
            WeatherUpdate(TICK_DT);
            FloodSceneUpdate(TICK_DT);

            if (sFadeDir == 0 && sScreens[sCurrent]->fixedUpdate)
            {
                sScreens[sCurrent]->fixedUpdate(TICK_DT);
            }

            sAccumulator -= TICK_DT;
            ticks++;
        }

        /* Too far behind to catch up: drop the debt rather than stack it. */
        if (ticks >= MAX_CATCHUP) sAccumulator = 0.0f;

        sAlpha = sAccumulator / TICK_DT;

        /* --- per-frame ------------------------------------------------- */
        if (sFadeDir > 0)
        {
            sFade += FADE_SPEED * frame;
            if (sFade >= 1.0f)
            {
                sFade = 1.0f;
                EnterScreen(sPending);
                sPending = SCREEN_NONE;
                sFadeDir = -1;
            }
        }
        else if (sFadeDir < 0)
        {
            sFade -= FADE_SPEED * frame;
            if (sFade <= 0.0f)
            {
                sFade = 0.0f;
                sFadeDir = 0;
            }
        }

        CursorUpdate(frame);

        /* Input is frozen mid-transition so a held key cannot double-fire. */
        if (sFadeDir == 0 && sScreens[sCurrent]->update) sScreens[sCurrent]->update(frame);

        BeginDrawing();
            ClearBackground(BLACK);

            if (!sScreens[sCurrent]->opaque)
            {
                /* Menus sit in the same place the game does. */
                FloodSceneDraw(1.0f, WeatherRain(), 0.0f);
                FilmVignette(0.7f);
            }
            if (sScreens[sCurrent]->draw) sScreens[sCurrent]->draw();

            if (sFade > 0.0f) DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, sFade));

            if (gSettings.showFps) DrawFPS(GetScreenWidth() - 96, 16);

            if (!sScreens[sCurrent]->hidesCursor) CursorDraw();
        EndDrawing();
    }
}

void AppShutdown(void)
{
    if (sCurrent != SCREEN_NONE && sScreens[sCurrent]->unload) sScreens[sCurrent]->unload();

    SettingsSave(SETTINGS_FILE);

    ThemeUnload();
    CloseAudioDevice();
    CloseWindow();
}
