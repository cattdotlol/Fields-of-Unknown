#include "ui/widgets.h"
#include "ui/theme.h"
#include "ui/cursor.h"
#include "core/input.h"

#include <math.h>

static bool PressedUp(void)      { return InputPressed(ACT_UP); }
static bool PressedDown(void)    { return InputPressed(ACT_DOWN); }
static bool PressedActivate(void){ return InputPressed(ACT_CONFIRM); }

/* Square wave rather than a sine: the blink reads as retro, and it never
   lands on a half-lit frame. */
static bool Blink(float hz)
{
    return fmodf((float)GetTime() * hz, 1.0f) < 0.5f;
}

/* One logical pixel at the current scale, never thinner than a real one. */
static float Px(void)
{
    return PixelScale();
}

/* Everything lands on whole pixels; no seams, no soft edges. */
static Rectangle Snap(Rectangle r)
{
    return (Rectangle){ floorf(r.x), floorf(r.y), floorf(r.width), floorf(r.height) };
}

bool NavVertical(int *focus, int count)
{
    int before = *focus;

    if (PressedUp())   *focus = (*focus - 1 + count) % count;
    if (PressedDown()) *focus = (*focus + 1) % count;

    return *focus != before;
}

/* Shared row chrome: fill, left marker, label. */
static void DrawRow(Rectangle b, const char *label, bool focused, float labelSize)
{
    b = Snap(b);

    if (focused)
    {
        DrawRectangleRec(b, Fade(gTheme.accent, 0.12f));

        float px = Px();
        Rectangle marker = Snap((Rectangle){ b.x, b.y, 3.0f * px, b.height });
        DrawRectangleRec(marker, gTheme.accent);
    }

    Vector2 m = UiMeasure(label, labelSize);
    UiText(label, b.x + 16.0f * ThemeScale(), b.y + (b.height - m.y) * 0.5f, labelSize,
           focused ? gTheme.text : gTheme.textDim);
}

bool WidgetButton(Rectangle bounds, const char *label, bool focused)
{
    float size = 20.0f * ThemeScale();
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) CursorHint();

    DrawRow(bounds, label, focused || hovered, size);

    return (focused && PressedActivate()) ||
           (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT));
}

bool WidgetToggle(Rectangle bounds, const char *label, bool *value, bool focused)
{
    float size = 20.0f * ThemeScale();
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) CursorHint();
    float s = ThemeScale();
    float px = Px();

    DrawRow(bounds, label, focused || hovered, size);

    /* Square track with a square knob - no radius anywhere. */
    float trackW = 54.0f * s;
    float trackH = 24.0f * s;
    Rectangle track = Snap((Rectangle){ bounds.x + bounds.width - trackW - 16.0f * s,
                                        bounds.y + (bounds.height - trackH) * 0.5f,
                                        trackW, trackH });

    DrawRectangleRec(track, *value ? gTheme.accentDim : gTheme.panel);
    DrawRectangleLinesEx(track, px, *value ? gTheme.accent : gTheme.border);

    float knobW = trackH - 8.0f * px;
    Rectangle knob = Snap((Rectangle){
        *value ? (track.x + track.width - knobW - 4.0f * px) : (track.x + 4.0f * px),
        track.y + 4.0f * px, knobW, knobW });

    DrawRectangleRec(knob, *value ? gTheme.accent : gTheme.textDim);

    bool changed = false;

    if (focused && (PressedActivate() || InputPressed(ACT_LEFT) || InputPressed(ACT_RIGHT)))
    {
        *value = !*value;
        changed = true;
    }
    if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT))
    {
        *value = !*value;
        changed = true;
    }

    return changed;
}

bool WidgetSlider(Rectangle bounds, const char *label, float *value, bool focused)
{
    float size = 20.0f * ThemeScale();
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    if (hovered) CursorHint();
    float s = ThemeScale();
    float px = Px();

    DrawRow(bounds, label, focused || hovered, size);

    /* Segmented bar - reads more like an old options menu than a line. */
    const int segments = 16;
    float barW = 200.0f * s;
    float barH = 16.0f * s;
    Rectangle bar = Snap((Rectangle){ bounds.x + bounds.width - barW - 74.0f * s,
                                      bounds.y + (bounds.height - barH) * 0.5f, barW, barH });

    float segW = bar.width / (float)segments;
    int filled = (int)(*value * (float)segments + 0.5f);

    for (int i = 0; i < segments; i++)
    {
        Rectangle cell = Snap((Rectangle){ bar.x + (float)i * segW, bar.y,
                                           segW - px, bar.height });

        if (i < filled) DrawRectangleRec(cell, focused ? gTheme.accent : gTheme.accentDim);
        else            DrawRectangleRec(cell, Fade(gTheme.border, 0.35f));
    }

    const char *pct = TextFormat("%3d%%", (int)(*value * 100.0f + 0.5f));
    UiText(pct, bounds.x + bounds.width - 64.0f * s, bounds.y + (bounds.height - size) * 0.5f,
           size, focused ? gTheme.text : gTheme.textDim);

    float before = *value;

    if (focused)
    {
        float step = GetFrameTime() * 0.9f;
        if (InputDown(ACT_LEFT))  *value -= step;
        if (InputDown(ACT_RIGHT)) *value += step;
    }

    Rectangle grab = { bar.x - 10.0f * s, bounds.y, bar.width + 20.0f * s, bounds.height };
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), grab))
    {
        *value = (GetMousePosition().x - bar.x) / bar.width;
    }

    if (*value < 0.0f) *value = 0.0f;
    if (*value > 1.0f) *value = 1.0f;

    return *value != before;
}

int WidgetMenu(const char **items, int count, int *selected, float centerX, float startY, float itemSize)
{
    float s = ThemeScale();
    float step = UiLineHeight(itemSize) * 1.5f;
    int activated = -1;

    NavVertical(selected, count);

    for (int i = 0; i < count; i++)
    {
        Vector2 m = UiMeasure(items[i], itemSize);
        float y = startY + (float)i * step;

        Rectangle box = Snap((Rectangle){ centerX - m.x * 0.5f - 28.0f * s, y - 8.0f * s,
                                          m.x + 56.0f * s, m.y + 16.0f * s });

        bool hovered = CheckCollisionPointRec(GetMousePosition(), box);
        if (hovered)
        {
            *selected = i;
            CursorHint();
        }

        bool active = (i == *selected);

        if (active)
        {
            DrawRectangleRec(box, Fade(gTheme.accent, 0.12f));
            DrawRectangleLinesEx(box, Px(), Fade(gTheme.accent, 0.55f));

            /* Blinking chevron, the way an old menu marks its cursor. */
            if (Blink(2.0f))
            {
                UiText(">", box.x - UiMeasure(">", itemSize).x - 12.0f * s, y, itemSize,
                       gTheme.accent);
            }
        }

        UiTextCentered(items[i], centerX, y, itemSize, active ? gTheme.text : gTheme.textDim);

        if (active && PressedActivate()) activated = i;
        if (hovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) activated = i;
    }

    return activated;
}
