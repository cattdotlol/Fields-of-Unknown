#include "screens/screens.h"
#include "core/app.h"
#include "core/input.h"
#include "core/settings.h"
#include "core/sysinfo.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include "raylib.h"

#include <math.h>

enum {
    ROW_MASTER = 0,
    ROW_MUSIC,
    ROW_SFX,
    ROW_FULLSCREEN,
    ROW_VSYNC,
    ROW_SHOWFPS,
    ROW_SHOWHUD,
    ROW_CONTROLS,
    ROW_BACK,
    ROW_COUNT
};

static int sFocus;

static void Init(void)
{
    sFocus = 0;
}

static void Update(float dt)
{
    (void)dt;

    NavVertical(&sFocus, ROW_COUNT);
    SysInfoRefreshDisplay();

    if (InputPressed(ACT_CANCEL)) AppGoTo(SCREEN_TITLE);
}

static float Px(void)
{
    float p = floorf(ThemeScale());
    return (p < 1.0f) ? 1.0f : p;
}

static void DrawPanel(Rectangle r, const char *heading)
{
    DrawRectangleRec(r, Fade(gTheme.panel, 0.88f));
    DrawRectangleLinesEx(r, Px(), gTheme.border);

    float s = ThemeScale();
    UiText(heading, r.x + 16.0f * s, r.y - 18.0f * s, 10.0f * s, gTheme.accent);
}

/* One label/value pair; the value wraps so long GPU strings stay inside
   the panel. Returns the height consumed. */
static float DrawInfoRow(const char *label, const char *value, float x, float y, float width)
{
    float s = ThemeScale();
    float used = 0.0f;

    UiText(label, x, y, 10.0f * s, gTheme.accent);
    used += UiLineHeight(10.0f * s);

    used += UiTextWrapped(value, x, y + used, width, 10.0f * s, gTheme.text);

    return used + 10.0f * s;
}

static void Draw(void)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float s = ThemeScale();
    float cx = w * 0.5f;

    UiTextCentered("SETTINGS", cx, floorf(h * 0.09f), 40.0f * s, gTheme.text);

    float leftW  = 600.0f * s;
    float rightW = 500.0f * s;
    float gap    = 24.0f * s;
    float totalW = leftW + rightW + gap;

    float rowH = 48.0f * s;
    float rowGap = 6.0f * s;
    float pad = 18.0f * s;

    float panelH = ROW_COUNT * (rowH + rowGap) + pad * 2.0f;
    float top = floorf(h * 0.22f);

    Rectangle left  = { floorf(cx - totalW * 0.5f), top, floorf(leftW), floorf(panelH) };
    Rectangle right = { floorf(left.x + leftW + gap), top, floorf(rightW), floorf(panelH) };

    DrawPanel(left, "OPTIONS");
    DrawPanel(right, "SYSTEM");

    /* --- options --- */
    Rectangle row = { left.x + pad, left.y + pad, left.width - pad * 2.0f, rowH };
    bool changed = false;

    changed |= WidgetSlider(row, "MASTER VOLUME", &gSettings.masterVolume, sFocus == ROW_MASTER);
    row.y += rowH + rowGap;
    changed |= WidgetSlider(row, "MUSIC VOLUME", &gSettings.musicVolume, sFocus == ROW_MUSIC);
    row.y += rowH + rowGap;
    changed |= WidgetSlider(row, "SFX VOLUME", &gSettings.sfxVolume, sFocus == ROW_SFX);
    row.y += rowH + rowGap;

    changed |= WidgetToggle(row, "FULLSCREEN", &gSettings.fullscreen, sFocus == ROW_FULLSCREEN);
    row.y += rowH + rowGap;
    changed |= WidgetToggle(row, "VSYNC", &gSettings.vsync, sFocus == ROW_VSYNC);
    row.y += rowH + rowGap;
    changed |= WidgetToggle(row, "SHOW FPS", &gSettings.showFps, sFocus == ROW_SHOWFPS);
    row.y += rowH + rowGap;
    changed |= WidgetToggle(row, "SHOW VITALS", &gSettings.showHud, sFocus == ROW_SHOWHUD);
    row.y += rowH + rowGap;

    if (WidgetButton(row, "CONTROLS", sFocus == ROW_CONTROLS)) AppGoTo(SCREEN_KEYBINDS);
    row.y += rowH + rowGap;

    if (WidgetButton(row, "BACK", sFocus == ROW_BACK)) AppGoTo(SCREEN_TITLE);

    if (changed) SettingsApply();

    /* --- hardware --- */
    float ix = right.x + pad;
    float iy = right.y + pad;
    float iw = right.width - pad * 2.0f;

    iy += DrawInfoRow("CPU", gSysInfo.cpu, ix, iy, iw);
    iy += DrawInfoRow("THREADS", TextFormat("%d", gSysInfo.cpuThreads), ix, iy, iw);
    iy += DrawInfoRow("MEMORY", gSysInfo.memory, ix, iy, iw);
    iy += DrawInfoRow("GPU", gSysInfo.gpu, ix, iy, iw);
    iy += DrawInfoRow("OPENGL", gSysInfo.glVersion, ix, iy, iw);
    iy += DrawInfoRow("GLSL", gSysInfo.glslVersion, ix, iy, iw);
    iy += DrawInfoRow("DISPLAY", gSysInfo.display, ix, iy, iw);
    iy += DrawInfoRow("RAYLIB", gSysInfo.raylibVersion, ix, iy, iw);

    UiTextCentered("LEFT / RIGHT  ADJUST      ESC  BACK", cx, h - 50.0f * s, 10.0f * s,
                   Fade(gTheme.textDim, 0.8f));
}

const Screen ScreenSettings = { .init = Init, .update = Update, .draw = Draw, .unload = NULL };
