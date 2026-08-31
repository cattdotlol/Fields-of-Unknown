#include "screens/screens.h"
#include "core/app.h"
#include "core/audio.h"
#include "core/config.h"
#include "core/devtools.h"
#include "core/settings.h"
#include "core/input.h"
#include "entity/aquatic.h"
#include "entity/cat.h"
#include "entity/rat.h"
#include "entity/stalker.h"
#include "entity/vitals.h"
#include "gfx/filmfx.h"
#include "gfx/lighting.h"
#include "gfx/scene_flood.h"
#include "ui/hud.h"
#include "ui/theme.h"
#include "world/terrain.h"
#include "world/worldgen.h"
#include "world/mushroom.h"
#include "world/ocean.h"
#include "world/season.h"
#include "world/weather.h"

#include "raylib.h"

#include <math.h>

/* World zoom lives in config.h so it tunes with the rest of the look. */
#define CAM_LAG    6.0f     /* higher = tighter follow */
#define CAM_LOOK   46.0f    /* lead the camera the way the cat is moving */

static Camera2D sCam;
static bool     sDebug;

/* The camera is smoothed in the fixed step and interpolated at draw time,
   exactly like the cat. Smoothing it per-frame against an already
   interpolated target meant the cat carried the interpolation jitter and
   the camera did not, so the world shook by up to one tick of movement
   whenever frame timing was uneven. */
static Vector2 sCamPrev;
static Vector2 sCamNow;

/* Dying used to be a silent teleport home: one frame at chunk five, the
   next at chunk zero, with nothing to say what had happened. */
static float sHurt;        /* red flash, decays          */
static float sDeath;       /* 0..1 fade out, then in     */
static bool  sReviving;
static float sLastHealth;

#define DEATH_FADE_OUT 1.4f
#define DEATH_FADE_IN  2.2f

/* Everything a fresh run resets, in one place. Death, the debug reroll
   and startup all go through here - they used to hand-roll the same
   list, and every new system was another line to forget. */
static void RestartRun(void)
{
    CatSpawn(WorldSpawnPoint());
    TerrainStream(CatPosition().x);

    VitalsReset();
    MushroomClearHarvests();
    RatsReset();
    StalkersReset();
    AquaticReset();

    sCamNow = CatPosition();
    sCamNow.y -= 24.0f;
    sCamPrev = sCamNow;
    sCam.target = sCamNow;

    sLastHealth = gVitals.health;
}

static void Init(void)
{
    WorldSetSeed(WORLD_SEED);
    RestartRun();

    sCam.rotation = 0.0f;
    sDebug = false;
}

/* Fixed step: deterministic, and in simulation space. */
static void CameraStep(float dt)
{
    Vector2 want = CatPosition();
    want.y -= 24.0f;

    /* Lead the way the cat is actually going. The old test compared the
       left edge of the box to its centre, which is always true, so the
       camera leaned right even when running left. */
    if (CatCurrentState() == CAT_RUN)
    {
        want.x += (CatVelocityX() > 0.0f) ? CAM_LOOK : -CAM_LOOK;
    }

    sCamPrev = sCamNow;

    sCamNow.x += (want.x - sCamNow.x) * CAM_LAG * dt;
    sCamNow.y += (want.y - sCamNow.y) * CAM_LAG * dt;
}

/* Per frame: same alpha the cat is drawn with, so the two never drift. */
static void CameraApply(void)
{
    float zoom = ThemeScale() * WORLD_ZOOM;
    if (zoom < 0.05f) zoom = 0.05f;   /* a minimised window reports zero */

    float a = AppRenderAlpha();

    sCam.zoom = zoom;
    sCam.offset = (Vector2){ (float)GetScreenWidth() * 0.5f,
                             (float)GetScreenHeight() * 0.55f };

    sCam.target.x = sCamPrev.x + (sCamNow.x - sCamPrev.x) * a;
    sCam.target.y = sCamPrev.y + (sCamNow.y - sCamPrev.y) * a;
}

static void FixedUpdate(float dt)
{
    if (DevFrozen()) return;

    /* --- dying ---------------------------------------------------------
       The world holds still while it fades, so the death reads as an
       event rather than a glitch. */
    if (gVitals.dead)
    {
        sDeath += dt / DEATH_FADE_OUT;

        if (sDeath >= 1.0f)
        {
            sDeath = 1.0f;
            RestartRun();
            sReviving = true;
        }

        return;
    }

    if (sReviving)
    {
        sDeath -= dt / DEATH_FADE_IN;

        if (sDeath <= 0.0f)
        {
            sDeath = 0.0f;
            sReviving = false;
        }
    }

    if (sHurt > 0.0f) sHurt -= dt * 1.8f;

    CatFixedUpdate(dt);

    /* Generate ahead of wherever the cat has got to. */
    TerrainStream(CatPosition().x);

    CameraStep(dt);

    VitalsUpdate(dt);
    MushroomTick(dt);

    /* Anything that took health off gets a flash and a thump. */
    if (gVitals.health < sLastHealth - 0.001f)
    {
        float lost = sLastHealth - gVitals.health;

        sHurt = 1.0f;
        AudioImpact(lost * 4.0f);
    }
    sLastHealth = gVitals.health;
    RatsFixedUpdate(dt);
    StalkersFixedUpdate(dt);
    AquaticFixedUpdate(dt);

    /* Eating is the only way hunger goes back up. What a species does is
       not written down anywhere - you find out by trying it. */
    if (InputPressed(ACT_EAT))
    {
        /* A rat is worth far more than a mushroom, so it wins the reach. */
        int rat = RatCatchable(CatBounds());

        if (rat >= 0)
        {
            RatConsume(rat);
            VitalsApply(0.45f, 0.05f, 0.10f);
        }
        else
        {
            int species = TerrainEatAt(CatBounds());

            if (species >= 0)
            {
                MushroomEffect e = MushroomEffectOf((unsigned char)species);
                VitalsApply(e.hunger, e.health, e.warmth);
            }
        }
    }

    /* Placeholder: back to the crash site. Dying should eventually cost
       something the player can feel. */
    if (gVitals.dead) RestartRun();
}

static void Update(float dt)
{
    if (IsKeyPressed(KEY_GRAVE)) DevToolsToggle();

    /* While the menu is up it owns the keyboard, or arrow keys would move
       the cat and browse the menu at the same time. */
    if (DevToolsUpdate(dt)) return;

    if (InputPressed(ACT_DEBUG)) sDebug = !sDebug;

    /* Dev: reroll the sprawl to eyeball generation variety. */
    if (IsKeyPressed(KEY_F5))
    {
        WorldSetSeed((unsigned int)GetTime() ^ (WorldSeed() * 2654435761u));
        RestartRun();
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
        TextFormat("RATS     %d  (%d fleeing)", RatCount(), RatAlarmed()),
        TextFormat("OCEAN    %s  depth %.0f  light %.2f",
                   OceanZoneName(OceanZoneAtDepth(OceanDepthAt(CatPosition().y))),
                   (double)OceanDepthAt(CatPosition().y),
                   (double)OceanLight(OceanDepthAt(CatPosition().y))),
        TextFormat("WATER    %d jelly  %d shark  %d whale",
                   AquaticCountOf(AQUA_JELLY), AquaticCountOf(AQUA_SHARK),
                   AquaticCountOf(AQUA_WHALE)),
        TextFormat("STALKERS %d  (%d hunting)  %.0f away", StalkerCount(),
                   StalkerHunting(), (double)StalkerNearestDistance()),
        TextFormat("SCENT M. %.2f", (double)WeatherScentMask()),
        TextFormat("SEASON   %s %.0f%%  temp %.2f", SeasonName(),
                   (double)(SeasonProgress() * 100.0f), (double)SeasonTemperature()),
        TextFormat("VITALS   hp %.2f food %.2f stam %.2f warm %.2f",
                   (double)gVitals.health, (double)gVitals.hunger,
                   (double)gVitals.stamina, (double)gVitals.warmth),
        TextFormat("SEED     %u", WorldSeed()),
        TextFormat("CHUNK    %d  [%d..%d]  %s", (int)floorf(CatPosition().x / CHUNK_WIDTH),
                   dbgFirst, dbgLast,
                   WorldDistrictName(WorldDistrictAt((int)floorf(CatPosition().x / CHUNK_WIDTH)))),
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

/* Anything solid overhead means the cat is inside or underground, which
   is the whole reason caves and apartments should look different. */
static bool UnderCover(void)
{
    Vector2 p = CatPosition();
    Rectangle probe = { p.x - 5.0f, p.y - 300.0f, 10.0f, 250.0f };

    return TerrainOverlaps(probe);
}

/* Lights the world, then multiplies the result over what was drawn. */
static void ApplyLighting(void)
{
    if (!DevLighting()) return;

    bool inside = UnderCover();

    /* Storms darken the day; being under cover darkens it far more. */
    float ambient = inside ? 0.16f : 0.46f;
    ambient -= WeatherRain() * 0.10f;
    ambient -= (1.0f - SeasonTemperature()) * 0.06f;

    /* Under water, light is whatever survives the depth. Exponential, so
       the twilight zone really is twilight and the midnight zone is
       nothing at all. */
    float depth = OceanDepthAt(CatPosition().y);
    if (depth > 0.0f) ambient *= OceanLight(depth);

    /* Lightning lights everything, briefly. */
    ambient += WeatherFlash() * 0.45f;

    if (ambient < 0.05f) ambient = 0.05f;

    LightingBegin(ambient);

    /* The cat sees in the dark; without this a cave is unplayable. */
    Vector2 eye = CatRenderPosition(AppRenderAlpha());
    eye.y -= 16.0f;

    LightingAddLight(sCam, eye, inside ? 300.0f : 210.0f,
                     (Color){ 214, 224, 210, 255 }, inside ? 0.70f : 0.34f);

    Vector2 viewL = GetScreenToWorld2D((Vector2){ 0.0f, 0.0f }, sCam);
    Vector2 viewR = GetScreenToWorld2D(
        (Vector2){ (float)GetScreenWidth(), (float)GetScreenHeight() }, sCam);

    int lit = 0;

    for (int i = 0; i < TerrainCount() && lit < 8; i++)
    {
        if (TerrainSolidKind(i) != SOLID_WALL) continue;

        Rectangle r = TerrainSolid(i);
        if (r.x + r.width < viewL.x || r.x > viewR.x) continue;

        /* Same rule the wall is drawn with, so the light sits on a window. */
        for (float wy = r.y + 26.0f; wy < r.y + r.height - 24.0f && lit < 8; wy += 80.0f)
        {
            if (sinf(wy * 0.21f + r.x * 0.07f) <= 0.55f) continue;

            LightingAddLight(sCam, (Vector2){ r.x + 7.0f, wy + 7.0f }, 190.0f,
                             (Color){ 220, 170, 96, 255 }, 0.60f);
            lit++;
        }
    }

    /* Jellyfish are the only light down there. */
    for (int i = 0; i < AQUATIC_MAX; i++)
    {
        float glow = AquaticGlow(i);
        if (glow <= 0.01f) continue;

        LightingAddLight(sCam, AquaticPosition(i), 130.0f,
                         (Color){ 150, 130, 226, 255 }, glow * 0.45f);
    }

    /* Vents: the only warm light on the sea floor. */
    for (int i = 0; i < TerrainVentCount(); i++)
    {
        Vector2 v = TerrainVent(i);
        if (v.x < viewL.x - 200.0f || v.x > viewR.x + 200.0f) continue;

        float flicker = 0.75f + 0.25f * sinf((float)GetTime() * 2.3f + v.x * 0.01f);

        LightingAddLight(sCam, v, 260.0f, (Color){ 236, 128, 70, 255 }, 0.55f * flicker);
    }

    /* And the thing that is looking for you. */
    for (int i = 0; i < STALKER_MAX; i++)
    {
        if (!StalkerActive(i)) continue;

        Vector2 p = StalkerPosition(i);
        p.y -= 30.0f;

        LightingAddLight(sCam, p, 150.0f, (Color){ 226, 132, 48, 255 },
                         0.30f + StalkerInterest(i) * 0.45f);
    }

    LightingEnd();
}

static void Draw(void)
{
    CameraApply();

    FloodSceneDraw(1.0f, WeatherRain(), sCam.target.x, WeatherIsSnow());

    Vector2 topLeft = GetScreenToWorld2D((Vector2){ 0.0f, 0.0f }, sCam);
    Vector2 botRight = GetScreenToWorld2D(
        (Vector2){ (float)GetScreenWidth(), (float)GetScreenHeight() }, sCam);

    BeginMode2D(sCam);
        TerrainDraw(topLeft.x, botRight.x, CatBounds());
        AquaticDraw(AppRenderAlpha(), topLeft.x, botRight.x);
        RatsDraw(AppRenderAlpha(), topLeft.x, botRight.x);
        StalkersDraw(AppRenderAlpha(), topLeft.x, botRight.x);
        CatDraw(AppRenderAlpha());

        /* Water last, so anything under it is tinted by it. */
        TerrainDrawWater(topLeft.x, botRight.x);

        if (DevShowHitboxes())
        {
            for (int i = 0; i < TerrainCount(); i++)
            {
                Rectangle r = TerrainSolid(i);
                if (r.x + r.width < topLeft.x || r.x > botRight.x) continue;

                DrawRectangleLinesEx(r, 1.0f, Fade(GREEN, 0.45f));
            }

            DrawRectangleLinesEx(CatBounds(), 1.5f, YELLOW);
        }

        if (DevShowAI())
        {
            for (int i = 0; i < RAT_MAX; i++)
            {
                if (!RatActive(i)) continue;

                Vector2 p = RatPosition(i);
                const char *tag = (RatCurrentState(i) == RAT_FLEE) ? "!"
                                : (RatCurrentState(i) == RAT_FREEZE) ? "?" : ".";

                DrawText(tag, (int)p.x - 2, (int)p.y - 30, 10, ORANGE);
                DrawRectangle((int)p.x - 10, (int)p.y - 18,
                              (int)(20.0f * RatAlertLevel(i)), 2, ORANGE);
            }

            for (int i = 0; i < STALKER_MAX; i++)
            {
                if (!StalkerActive(i)) continue;

                Vector2 p = StalkerPosition(i);
                DrawRectangle((int)p.x - 20, (int)p.y - 46,
                              (int)(40.0f * StalkerInterest(i)), 3, RED);
                DrawLineV(p, CatPosition(), Fade(RED, 0.25f));
            }
        }
    EndMode2D();

    /* Lighting goes on before the effects and the interface, so neither
       gets dimmed by it. */
    ApplyLighting();

    FilmVignette(0.6f);

    /* Taking damage: a red wash, so a hit is never invisible. */
    if (sHurt > 0.0f)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade((Color){ 168, 32, 32, 255 }, sHurt * 0.30f));
    }

    /* Lightning: the flash lands now, the sound arrives later. */
    float flash = WeatherFlash();
    if (flash > 0.0f)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(),
                      Fade(RAYWHITE, flash * 0.42f));
    }

    if (gSettings.showHud && sDeath < 0.9f) HudDraw();

    /* Death fade goes over everything, including the HUD. */
    if (sDeath > 0.0f)
    {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, sDeath));
    }

    if (sDebug) DrawDebug();
    DevToolsDraw();
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
