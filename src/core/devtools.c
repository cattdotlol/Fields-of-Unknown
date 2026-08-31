#include "core/devtools.h"

#include "raylib.h"

#if defined(NDEBUG)

/* Release: nothing here at all. */
void DevToolsToggle(void) { }
bool DevToolsOpen(void) { return false; }
bool DevToolsUpdate(float dt) { (void)dt; return false; }
void DevToolsDraw(void) { }
bool DevGodMode(void) { return false; }
bool DevFrozen(void) { return false; }
bool DevShowHitboxes(void) { return false; }
bool DevShowAI(void) { return false; }

#else

#include "core/input.h"
#include "entity/cat.h"
#include "entity/rat.h"
#include "entity/stalker.h"
#include "entity/vitals.h"
#include "ui/theme.h"
#include "world/season.h"
#include "world/terrain.h"
#include "world/weather.h"
#include "world/worldgen.h"

#include <math.h>

typedef enum Row {
    ROW_GOD = 0,
    ROW_FREEZE,
    ROW_HITBOX,
    ROW_AI,
    ROW_WEATHER,
    ROW_SEASON,
    ROW_WATER,
    ROW_HEAL,
    ROW_SPAWN_RAT,
    ROW_SPAWN_STALKER,
    ROW_WARP,
    ROW_COUNT
} Row;

static bool sOpen;
static int  sRow;

static bool sGod;
static bool sFrozen;
static bool sHitboxes;
static bool sAI;

bool DevGodMode(void)      { return sGod; }
bool DevFrozen(void)       { return sFrozen; }
bool DevShowHitboxes(void) { return sHitboxes; }
bool DevShowAI(void)       { return sAI; }

bool DevToolsOpen(void) { return sOpen; }

void DevToolsToggle(void)
{
    sOpen = !sOpen;

    /* Never leave the world paused behind a closed menu. */
    if (!sOpen) sFrozen = false;
}

static const char *WEATHER_NAMES[] = { "DRY", "DRIZZLE", "RAIN", "STORM" };
static const char *SEASON_NAMES[]  = { "SPRING", "SUMMER", "AUTUMN", "WINTER" };

/* Left/right on a row; +1 or -1. */
static void Adjust(int delta)
{
    switch (sRow)
    {
        case ROW_WEATHER:
        {
            int next = ((int)WeatherCurrent() + delta + WEATHER_STATE_COUNT) % WEATHER_STATE_COUNT;
            WeatherForceState((WeatherState)next);
            break;
        }

        case ROW_SEASON:
        {
            int next = ((int)SeasonCurrent() + delta + SEASON_COUNT) % SEASON_COUNT;
            SeasonSet((Season)next);
            break;
        }

        case ROW_WATER:
            WeatherSetWetness(WeatherWetness() + (float)delta * 0.1f);
            break;

        case ROW_WARP:
        {
            /* A chunk at a time, landing on the ground rather than in it. */
            float x = CatPosition().x + (float)delta * CHUNK_WIDTH;
            TerrainStream(x);
            CatSpawn((Vector2){ x, WorldEdgeHeight((int)floorf(x / CHUNK_WIDTH)) });
            break;
        }

        default: break;
    }
}

static void Activate(void)
{
    switch (sRow)
    {
        case ROW_GOD:    sGod = !sGod; break;
        case ROW_FREEZE: sFrozen = !sFrozen; break;
        case ROW_HITBOX: sHitboxes = !sHitboxes; break;
        case ROW_AI:     sAI = !sAI; break;

        case ROW_HEAL:   VitalsReset(); break;

        case ROW_SPAWN_RAT:     RatsForceSpawn(CatPosition().x + 120.0f); break;
        case ROW_SPAWN_STALKER: StalkersForceSpawn(CatPosition().x + 420.0f); break;

        default: Adjust(1); break;
    }
}

static const char *ValueFor(Row row)
{
    switch (row)
    {
        case ROW_GOD:     return sGod ? "ON" : "off";
        case ROW_FREEZE:  return sFrozen ? "ON" : "off";
        case ROW_HITBOX:  return sHitboxes ? "ON" : "off";
        case ROW_AI:      return sAI ? "ON" : "off";
        case ROW_WEATHER: return WEATHER_NAMES[WeatherCurrent()];
        case ROW_SEASON:  return SEASON_NAMES[SeasonCurrent()];
        case ROW_WATER:   return TextFormat("%.0f%%", (double)(WeatherWetness() * 100.0f));
        case ROW_WARP:    return TextFormat("chunk %d",
                                            (int)floorf(CatPosition().x / CHUNK_WIDTH));
        default:          return "";
    }
}

static const char *LABELS[ROW_COUNT] = {
    "GOD MODE", "FREEZE WORLD", "HITBOXES", "SHOW AI",
    "WEATHER", "SEASON", "WATER LEVEL",
    "REFILL VITALS", "SPAWN RAT", "SPAWN STALKER", "WARP",
};

bool DevToolsUpdate(float dt)
{
    (void)dt;

    if (!sOpen) return false;

    if (IsKeyPressed(KEY_UP) || IsKeyPressed(KEY_W))   sRow = (sRow - 1 + ROW_COUNT) % ROW_COUNT;
    if (IsKeyPressed(KEY_DOWN) || IsKeyPressed(KEY_S)) sRow = (sRow + 1) % ROW_COUNT;

    if (IsKeyPressed(KEY_LEFT))  Adjust(-1);
    if (IsKeyPressed(KEY_RIGHT)) Adjust(1);

    if (IsKeyPressed(KEY_ENTER) || IsKeyPressed(KEY_SPACE)) Activate();

    /* God mode is a floor under the vitals, applied continuously. */
    if (sGod) VitalsApply(1.0f, 1.0f, 1.0f);

    return true;
}

void DevToolsDraw(void)
{
    if (!sOpen) return;

    float s = ThemeScale();
    float w = 300.0f * s;
    float rowH = 26.0f * s;
    float pad = 16.0f * s;
    float h = ROW_COUNT * rowH + pad * 3.0f + 30.0f * s;

    float x = (float)GetScreenWidth() - w - 24.0f * s;
    float y = 24.0f * s;

    DrawRectangleRec((Rectangle){ x, y, w, h }, Fade((Color){ 8, 10, 16, 255 }, 0.92f));
    DrawRectangleLinesEx((Rectangle){ x, y, w, h }, 1.0f * s, gTheme.accent);

    UiText("DEV", x + pad, y + pad * 0.5f, 20.0f * s, gTheme.accent);
    UiText(TextFormat("%s  seed %u", WorldDistrictName(
               WorldDistrictAt((int)floorf(CatPosition().x / CHUNK_WIDTH))), WorldSeed()),
           x + pad, y + pad * 0.5f + 20.0f * s, 10.0f * s, gTheme.textDim);

    float ry = y + pad + 34.0f * s;

    for (int i = 0; i < ROW_COUNT; i++)
    {
        bool on = (sRow == i);

        if (on)
        {
            DrawRectangleRec((Rectangle){ x + 6.0f * s, ry - 3.0f * s,
                                          w - 12.0f * s, rowH }, Fade(gTheme.accent, 0.14f));
        }

        UiText(LABELS[i], x + pad, ry, 10.0f * s, on ? gTheme.text : gTheme.textDim);

        const char *value = ValueFor((Row)i);
        Vector2 m = UiMeasure(value, 10.0f * s);
        UiText(value, x + w - pad - m.x, ry, 10.0f * s,
               on ? gTheme.accent : gTheme.textDim);

        ry += rowH;
    }

    UiText("ARROWS  move / adjust      ENTER  do      ~  close",
           x + pad, y + h - pad, 10.0f * s, Fade(gTheme.textDim, 0.7f));
}

#endif /* NDEBUG */
