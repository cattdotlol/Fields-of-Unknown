#include "screens/screens.h"
#include "core/app.h"
#include "core/config.h"
#include "core/settings.h"
#include "core/input.h"
#include "entity/cat.h"
#include "entity/vitals.h"
#include "gfx/filmfx.h"
#include "gfx/scene_flood.h"
#include "ui/hud.h"
#include "ui/theme.h"
#include "world/terrain.h"
#include "world/worldgen.h"
#include "world/mushroom.h"
#include "world/season.h"
#include "world/weather.h"

#include "raylib.h"

#include <math.h>

/* World zoom lives in config.h so it tunes with the rest of the look. */
#define CAM_LAG    6.0f     /* higher = tighter follow */
#define CAM_LOOK   46.0f    /* lead the camera the way the cat is moving */

static Camera2D sCam;
static bool     sDebug;

static void Init(void)
{
    WorldSetSeed(WORLD_SEED);
    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);
    VitalsReset();
    MushroomClearHarvests();

    sCam.target = CatPosition();
    sCam.rotation = 0.0f;
    sDebug = false;
}

static void FollowCat(float dt)
{
    float zoom = ThemeScale() * WORLD_ZOOM;

    sCam.zoom = zoom;
    sCam.offset = (Vector2){ (float)GetScreenWidth() * 0.5f, (float)GetScreenHeight() * 0.55f };

    Vector2 want = CatRenderPosition(AppRenderAlpha());
    want.y -= 24.0f;

    /* Look ahead slightly, so running does not crowd the screen edge. */
    if (CatCurrentState() == CAT_RUN) want.x += CAM_LOOK * ((CatBounds().x < want.x) ? 1.0f : -1.0f);

    sCam.target.x += (want.x - sCam.target.x) * CAM_LAG * dt;
    sCam.target.y += (want.y - sCam.target.y) * CAM_LAG * dt;

}

static void FixedUpdate(float dt)
{
    CatFixedUpdate(dt);

    /* Generate ahead of wherever the cat has got to. */
    TerrainStream(CatPosition().x);

    VitalsUpdate(dt);
    MushroomTick(dt);

    /* Eating is the only way hunger goes back up. What a species does is
       not written down anywhere - you find out by trying it. */
    if (InputPressed(ACT_EAT))
    {
        int species = TerrainEatAt(CatBounds());

        if (species >= 0)
        {
            MushroomEffect e = MushroomEffectOf((unsigned char)species);
            VitalsApply(e.hunger, e.health, e.warmth);
        }
    }

    /* Placeholder: back to the crash site. Dying should eventually cost
       something the player can feel. */
    if (gVitals.dead)
    {
        CatSpawn(WorldSpawnPoint());
        TerrainStream(CatPosition().x);
        VitalsReset();
        MushroomClearHarvests();
    }
}

static void Update(float dt)
{
    /* Camera runs per-frame off the interpolated position, so it stays
       smooth on displays faster than the tick rate. */
    FollowCat(dt);

    if (InputPressed(ACT_DEBUG)) sDebug = !sDebug;

    /* Dev: reroll the sprawl to eyeball generation variety. */
    if (IsKeyPressed(KEY_F5))
    {
        WorldSetSeed((unsigned int)GetTime() ^ (WorldSeed() * 2654435761u));
        CatSpawn(WorldSpawnPoint());
        TerrainStream(CatPosition().x);
        VitalsReset();
        MushroomClearHarvests();
        sCam.target = CatPosition();
    }
    if (InputPressed(ACT_CANCEL)) AppGoTo(SCREEN_TITLE);
}

static const char *StateName(CatState s)
{
    switch (s)
    {
        case CAT_IDLE:   return "IDLE";
        case CAT_WALK:   return "WALK";
        case CAT_RUN:    return "RUN";
        case CAT_CROUCH: return "CROUCH";
        case CAT_AIR:    return "AIR";
        case CAT_SWIM:   return "SWIM";
        default:         return "?";
    }
}

/* Dev-only. The game itself shows no meters - see the cat, not a number. */
static void DrawDebug(void)
{
    int dbgFirst = 0, dbgLast = 0;
    TerrainLoadedRange(&dbgFirst, &dbgLast);

    float s = ThemeScale();
    float x = 24.0f * s;
    float y = 24.0f * s;
    float step = UiLineHeight(10.0f * s);

    const char *lines[] = {
        TextFormat("WEATHER  %s", WeatherName()),
        TextFormat("RAIN     %.2f", (double)WeatherRain()),
        TextFormat("WETNESS  %.2f", (double)WeatherWetness()),
        TextFormat("WATER Y  %.0f", (double)WeatherWaterY()),
        TextFormat("WIND     %+.2f", (double)WeatherWind()),
        TextFormat("STATE    %s", StateName(CatCurrentState())),
        TextFormat("NOISE    %.2f", (double)CatNoise()),
        TextFormat("SCENT M. %.2f", (double)WeatherScentMask()),
        TextFormat("SEASON   %s %.0f%%  temp %.2f", SeasonName(),
                   (double)(SeasonProgress() * 100.0f), (double)SeasonTemperature()),
        TextFormat("VITALS   hp %.2f food %.2f stam %.2f warm %.2f",
                   (double)gVitals.health, (double)gVitals.hunger,
                   (double)gVitals.stamina, (double)gVitals.warmth),
        TextFormat("SEED     %u", WorldSeed()),
        TextFormat("CHUNK    %d  [%d..%d]", (int)floorf(CatPosition().x / CHUNK_WIDTH),
                   dbgFirst, dbgLast),
        TextFormat("SOLIDS   %d in %d chunks", TerrainCount(), TerrainLoadedChunks()),
        TextFormat("X        %.0f", (double)CatPosition().x),
    };

    int count = (int)(sizeof(lines) / sizeof(lines[0]));

    DrawRectangle((int)(x - 10.0f * s), (int)(y - 10.0f * s),
                  (int)(240.0f * s), (int)(step * (float)count + 20.0f * s),
                  Fade(BLACK, 0.7f));

    for (int i = 0; i < count; i++)
    {
        UiText(lines[i], x, y + (float)i * step, 10.0f * s, gTheme.accent);
    }
}

static void Draw(void)
{
    FloodSceneDraw(1.0f, WeatherRain(), sCam.target.x, WeatherIsSnow());

    Vector2 topLeft = GetScreenToWorld2D((Vector2){ 0.0f, 0.0f }, sCam);
    Vector2 botRight = GetScreenToWorld2D(
        (Vector2){ (float)GetScreenWidth(), (float)GetScreenHeight() }, sCam);

    BeginMode2D(sCam);
        TerrainDraw(topLeft.x, botRight.x, CatBounds());
        CatDraw(AppRenderAlpha());

        /* Water last, so anything under it is tinted by it. */
        TerrainDrawWater(topLeft.x, botRight.x);
    EndMode2D();

    FilmVignette(0.6f);

    /* Lightning: the flash lands now, the sound arrives later. */
    float flash = WeatherFlash();
    if (flash > 0.0f)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade(RAYWHITE, flash * 0.42f));
    }

    if (gSettings.showHud) HudDraw();
    if (sDebug) DrawDebug();
}

const Screen ScreenGameplay = {
    .init = Init,
    .update = Update,
    .fixedUpdate = FixedUpdate,
    .draw = Draw,
    .unload = NULL,
    .opaque = true,
    .hidesCursor = true,
};
