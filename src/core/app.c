#include "core/app.h"
#include "core/config.h"
#include "core/audio.h"
#include "core/input.h"
#include "core/settings.h"
#include "core/sysinfo.h"
#include "core/timestep.h"
#include "gfx/filmfx.h"
#include "gfx/lighting.h"
#include "gfx/scene_flood.h"
#include "world/season.h"
#include "world/daylight.h"
#include "world/weather.h"
#include "ui/theme.h"
#include "ui/cursor.h"

#include "raylib.h"

#define FADE_SPEED 3.2f

#define TICK_DT ((float)SIM_TICK_DT)

/* Registry index must match the ScreenId enum order. */
static const Screen *sScreens[SCREEN_COUNT];

static ScreenId sCurrent = SCREEN_NONE;
static ScreenId sPending = SCREEN_NONE;
static float    sFade;          /* 0 = clear, 1 = black */
static int      sFadeDir;       /* +1 out, -1 in, 0 idle */
static bool     sQuit;
static TimeStep sClock;

static void EnterScreen(ScreenId id)
{
    InputClearPending();
    if (sCurrent != SCREEN_NONE && sScreens[sCurrent]->unload) sScreens[sCurrent]->unload();

    sCurrent = id;

    if (sCurrent != SCREEN_NONE && sScreens[sCurrent]->init) sScreens[sCurrent]->init();
}

void AppGoTo(ScreenId id)
{
    if (id < 0 || id >= SCREEN_COUNT) return;
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
    return sClock.alpha;
}

void AppInit(void)
{
    sClock = (TimeStep){0};
    sQuit = false;
    sFade = 0.0f;
    sFadeDir = 0;
    sPending = SCREEN_NONE;
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
    LightingLoad();
    AudioLoad();
    CursorLoad();
    SysInfoGather();          /* needs the GL context InitWindow created */
    FloodSceneInit(WORLD_SEED);
    SeasonInit();
    WeatherInit(WORLD_SEED);
    DaylightInit();

    sScreens[SCREEN_TITLE]    = &ScreenTitle;
    sScreens[SCREEN_INTRO]    = &ScreenIntro;
    sScreens[SCREEN_SETTINGS] = &ScreenSettings;
    sScreens[SCREEN_KEYBINDS] = &ScreenKeybinds;
    sScreens[SCREEN_GAMEPLAY] = &ScreenGameplay;

    EnterScreen(SCREEN_TITLE);
}

void AppRun(void)
{
    while (!WindowShouldClose() && !sQuit)
    {
        float frame = GetFrameTime();
        int ticks = TimeStepAdvance(&sClock, frame);
        if (frame > SIM_MAX_FRAME) frame = (float)SIM_MAX_FRAME;

        InputPoll();
        if (sFadeDir != 0 || sCurrent != SCREEN_GAMEPLAY) InputClearPending();

        /* --- fixed-rate simulation ------------------------------------ */
        for (int tick = 0; tick < ticks; tick++)
        {
            SeasonUpdate(TICK_DT);
            WeatherUpdate(TICK_DT);
            DaylightUpdate(TICK_DT);
            FloodSceneUpdate(TICK_DT);

            if (sFadeDir == 0 && sScreens[sCurrent]->fixedUpdate)
            {
                sScreens[sCurrent]->fixedUpdate(TICK_DT);
            }
        }

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

        AudioUpdate();
        CursorUpdate(frame);

        /* Input is frozen mid-transition so a held key cannot double-fire. */
        if (sFadeDir == 0 && sScreens[sCurrent]->update) sScreens[sCurrent]->update(frame);

        BeginDrawing();
            ClearBackground(BLACK);

            if (!sScreens[sCurrent]->opaque)
            {
                /* Menus sit in the same place the game does. */
                FloodSceneDraw(1.0f, WeatherRain(), 0.0f, WeatherIsSnow());
                FilmVignette(0.7f);

                float flash = WeatherFlash();
                if (flash > 0.0f)
                {
                    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                                  Fade(RAYWHITE, flash * 0.30f));
                }
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
    sCurrent = SCREEN_NONE;

    SettingsSave(SETTINGS_FILE);

    AudioUnload();
    LightingUnload();
    ThemeUnload();
    CloseAudioDevice();
    CloseWindow();
}
