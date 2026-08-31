#include "screens/screens.h"
#include "core/app.h"
#include "core/input.h"
#include "ui/theme.h"
#include "ui/widgets.h"

#include "raylib.h"

#include <math.h>

/* Two bindings per action is enough for a keyboard and leaves the third
   slot free for whatever a gamepad remap needs later. */
#define SLOTS 2

static InputAction sRows[ACT_COUNT];
static int  sRowCount;
static int  sRow;
static int  sCol;
static bool sCapturing;
static InputAction sClashed;    /* what we stole a key from, for a moment */
static float sClashTimer;

/* The two rows past the bindings. */
#define ROW_RESET (sRowCount)
#define ROW_BACK  (sRowCount + 1)
#define ROW_TOTAL (sRowCount + 2)

static void Init(void)
{
    sRowCount = 0;

    for (int a = 0; a < ACT_COUNT; a++)
    {
        if (InputActionRebindable((InputAction)a)) sRows[sRowCount++] = (InputAction)a;
    }

    sRow = 0;
    sCol = 0;
    sCapturing = false;
    sClashed = ACT_COUNT;
    sClashTimer = 0.0f;
}

static void BeginCapture(void)
{
    sCapturing = true;

    /* Drain the queue, or the Enter that opened capture is the key we
       would immediately bind. */
    while (GetKeyPressed() != 0) { }
}

static void Assign(int key)
{
    InputAction action = sRows[sRow];

    /* A key can only mean one thing: take it off whatever had it. */
    InputAction other = InputActionUsing(key, action);

    if (other != ACT_COUNT)
    {
        for (int i = 0; i < INPUT_MAX_BINDINGS; i++)
        {
            if (InputBinding(other, i) == key) InputBind(other, i, 0);
        }

        sClashed = other;
        sClashTimer = 2.0f;
    }

    InputBind(action, sCol, key);
}

static void Update(float dt)
{
    if (sClashTimer > 0.0f) sClashTimer -= dt;

    if (sCapturing)
    {
        int key = GetKeyPressed();

        if (key == KEY_ESCAPE) { sCapturing = false; return; }

        if (key != 0)
        {
            Assign(key);
            sCapturing = false;
        }

        return;     /* nothing else reads input while we are listening */
    }

    if (InputPressed(ACT_UP))    sRow = (sRow - 1 + ROW_TOTAL) % ROW_TOTAL;
    if (InputPressed(ACT_DOWN))  sRow = (sRow + 1) % ROW_TOTAL;
    if (InputPressed(ACT_LEFT))  sCol = (sCol - 1 + SLOTS) % SLOTS;
    if (InputPressed(ACT_RIGHT)) sCol = (sCol + 1) % SLOTS;

    if (InputPressed(ACT_CONFIRM))
    {
        if (sRow == ROW_RESET)     InputResetDefaults();
        else if (sRow == ROW_BACK) AppGoTo(SCREEN_SETTINGS);
        else                       BeginCapture();
    }

    /* Clearing a binding outright, rather than having to rebind it. */
    if (sRow < sRowCount && (IsKeyPressed(KEY_DELETE) || IsKeyPressed(KEY_BACKSPACE)))
    {
        InputBind(sRows[sRow], sCol, 0);
    }

    if (InputPressed(ACT_CANCEL)) AppGoTo(SCREEN_SETTINGS);
}

static float Px(void)
{
    float p = floorf(ThemeScale());
    return (p < 1.0f) ? 1.0f : p;
}

static void Draw(void)
{
    float w = (float)GetScreenWidth();
    float h = (float)GetScreenHeight();
    float s = ThemeScale();
    float cx = w * 0.5f;

    UiTextCentered("CONTROLS", cx, floorf(h * 0.07f), 40.0f * s, gTheme.text);

    float panelW = 620.0f * s;
    float rowH = 40.0f * s;
    float rowGap = 4.0f * s;
    float pad = 18.0f * s;
    float panelH = ROW_TOTAL * (rowH + rowGap) + pad * 2.0f;

    Rectangle panel = { floorf(cx - panelW * 0.5f), floorf(h * 0.18f),
                        floorf(panelW), floorf(panelH) };

    DrawRectangleRec(panel, Fade(gTheme.panel, 0.88f));
    DrawRectangleLinesEx(panel, Px(), gTheme.border);

    float cellW = 150.0f * s;
    float y = panel.y + pad;

    for (int i = 0; i < sRowCount; i++)
    {
        bool onRow = (sRow == i);

        Rectangle row = { panel.x + pad, y, panel.width - pad * 2.0f, rowH };
        if (onRow) DrawRectangleRec(row, Fade(gTheme.accent, 0.10f));

        UiText(InputActionName(sRows[i]), row.x + 12.0f * s,
               row.y + (rowH - 20.0f * s) * 0.5f, 20.0f * s,
               onRow ? gTheme.text : gTheme.textDim);

        for (int c = 0; c < SLOTS; c++)
        {
            Rectangle cell = { row.x + row.width - cellW * (float)(SLOTS - c) - 8.0f * s,
                               row.y + 5.0f * s, cellW - 8.0f * s, rowH - 10.0f * s };

            bool focused = onRow && (sCol == c);
            bool listening = focused && sCapturing;

            DrawRectangleRec(cell, Fade(gTheme.accentDim, focused ? 0.35f : 0.15f));
            DrawRectangleLinesEx(cell, Px(), focused ? gTheme.accent : gTheme.border);

            const char *label = InputKeyName(InputBinding(sRows[i], c));

            /* Blink while waiting, so it is obvious the game is listening. */
            if (listening)
            {
                label = (fmodf((float)GetTime() * 2.5f, 1.0f) < 0.5f) ? "PRESS KEY" : "";
            }

            Vector2 m = UiMeasure(label, 16.0f * s);
            UiText(label, cell.x + (cell.width - m.x) * 0.5f,
                   cell.y + (cell.height - m.y) * 0.5f, 16.0f * s,
                   listening ? gTheme.accent : gTheme.text);
        }

        y += rowH + rowGap;
    }

    const char *tail[2] = { "RESET TO DEFAULTS", "BACK" };

    for (int i = 0; i < 2; i++)
    {
        bool onRow = (sRow == sRowCount + i);
        Rectangle row = { panel.x + pad, y, panel.width - pad * 2.0f, rowH };

        if (onRow) DrawRectangleRec(row, Fade(gTheme.accent, 0.10f));

        UiText(tail[i], row.x + 12.0f * s, row.y + (rowH - 20.0f * s) * 0.5f,
               20.0f * s, onRow ? gTheme.text : gTheme.textDim);

        y += rowH + rowGap;
    }

    if (sClashTimer > 0.0f && sClashed != ACT_COUNT)
    {
        UiTextCentered(TextFormat("UNBOUND FROM %s", InputActionName(sClashed)),
                       cx, panel.y + panel.height + 16.0f * s, 10.0f * s,
                       Fade(gTheme.accent, sClashTimer * 0.5f));
    }

    UiTextCentered(sCapturing ? "ESC  CANCEL"
                              : "ENTER  REBIND      DEL  CLEAR      ESC  BACK",
                   cx, h - 50.0f * s, 10.0f * s, Fade(gTheme.textDim, 0.8f));
}

const Screen ScreenKeybinds = {
    .init = Init,
    .update = Update,
    .draw = Draw,
    .unload = NULL,
};
