#include "ui/theme.h"
#include "core/config.h"

#include <math.h>
#include <stddef.h>

Theme gTheme;

void ThemeLoad(void)
{
    gTheme.text      = (Color){ 236, 240, 250, 255 };
    gTheme.textDim   = (Color){ 124, 136, 164, 255 };
    gTheme.accent    = (Color){  96, 220, 232, 255 };
    gTheme.accentDim = (Color){  40, 104, 124, 255 };
    gTheme.panel     = (Color){  10,  12,  24, 255 };
    gTheme.border    = (Color){  62,  86, 112, 255 };

    /* raylib's built-in font is a 10px bitmap face - the classic pixel
       look, and it costs nothing to ship. Point filtering keeps the
       glyph edges hard when it is scaled up. */
    gTheme.font = GetFontDefault();
    SetTextureFilter(gTheme.font.texture, TEXTURE_FILTER_POINT);
}

void ThemeUnload(void)
{
    /* The default font is owned by raylib; nothing to release. */
}

float ThemeScale(void)
{
    return (float)GetScreenHeight() / DESIGN_HEIGHT;
}

float PixelScale(void)
{
    float p = floorf(ART_PIXEL * ThemeScale());
    return (p < 1.0f) ? 1.0f : p;
}

float UiSnap(float size)
{
    float base = (float)gTheme.font.baseSize;
    if (base <= 0.0f) base = 10.0f;

    float steps = floorf(size / base + 0.5f);
    if (steps < 1.0f) steps = 1.0f;

    return base * steps;
}

/* Spacing must also be a whole number of pixels or the grid drifts. */
static float Spacing(float snapped)
{
    float base = (float)gTheme.font.baseSize;
    if (base <= 0.0f) base = 10.0f;

    return floorf(snapped / base);
}

float UiLineHeight(float size)
{
    float snapped = UiSnap(size);
    return snapped + Spacing(snapped) * 2.0f;
}

Vector2 UiMeasure(const char *text, float size)
{
    float snapped = UiSnap(size);
    return MeasureTextEx(gTheme.font, text, snapped, Spacing(snapped));
}

void UiText(const char *text, float x, float y, float size, Color color)
{
    float snapped = UiSnap(size);

    /* Integer positions - a half-pixel offset is what makes a pixel font
       look blurry, even with point filtering. */
    Vector2 at = { floorf(x), floorf(y) };

    DrawTextEx(gTheme.font, text, at, snapped, Spacing(snapped), color);
}

void UiTextCentered(const char *text, float cx, float y, float size, Color color)
{
    Vector2 m = UiMeasure(text, size);
    UiText(text, cx - m.x * 0.5f, y, size, color);
}

float UiTextWrapped(const char *text, float x, float y, float maxWidth, float size, Color color)
{
    char line[256] = { 0 };
    char word[128];
    char candidate[256];

    float lineH = UiLineHeight(size);
    float startY = y;
    const char *p = text;

    for (;;)
    {
        while (*p == ' ') p++;

        int wi = 0;
        while (*p != '\0' && *p != ' ' && wi < (int)sizeof(word) - 1) word[wi++] = *p++;
        word[wi] = '\0';

        if (wi == 0) break;

        if (line[0] == '\0') TextCopy(candidate, word);
        else                 TextCopy(candidate, TextFormat("%s %s", line, word));

        if (line[0] != '\0' && UiMeasure(candidate, size).x > maxWidth)
        {
            UiText(line, x, y, size, color);
            y += lineH;
            TextCopy(line, word);
        }
        else
        {
            TextCopy(line, candidate);
        }
    }

    if (line[0] != '\0')
    {
        UiText(line, x, y, size, color);
        y += lineH;
    }

    return y - startY;
}

const char *AssetPath(const char *relative)
{
    static char resolved[512];

    if (FileExists(relative)) return relative;

    TextCopy(resolved, TextFormat("%s../%s", GetApplicationDirectory(), relative));
    if (FileExists(resolved)) return resolved;

    TextCopy(resolved, TextFormat("%s%s", GetApplicationDirectory(), relative));
    if (FileExists(resolved)) return resolved;

    return relative;
}
